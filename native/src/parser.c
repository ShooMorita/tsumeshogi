#include "tsume_shogi.h"

#include <stdio.h>
#include <string.h>

static int utf8_byte_len(unsigned char firstByte)
{
    if ((firstByte & 0x80) == 0x00)
        return 1;
    if ((firstByte & 0xE0) == 0xC0)
        return 2;
    if ((firstByte & 0xF0) == 0xE0)
        return 3;
    if ((firstByte & 0xF8) == 0xF0)
        return 4;
    return 1;
}

static bool is_delimiter(char character)
{
    return character == '\0' || character == ' ' || character == '\n' || character == '\r';
}

static int consume_ten_if_present(const char** countText)
{
    const size_t juLen = strlen("十");
    if (strncmp(*countText, "十", juLen) != 0)
        return 0;
    *countText += juLen;
    return 10;
}

static int parse_count(const char* countText)
{
    if (!countText || is_delimiter(*countText))
        return 1;
    if ('0' <= countText[0] && countText[0] <= '9')
        return countText[0] - '0';

    int count = consume_ten_if_present(&countText);
    if (is_delimiter(*countText))
        return count > 0 ? count : 1;

    const char* kanjiDigits[] = { "", "一", "二", "三", "四", "五", "六", "七", "八", "九" };
    for (int digit = 1; digit <= 9; digit++) {
        size_t digitLen = strlen(kanjiDigits[digit]);
        if (strncmp(countText, kanjiDigits[digit], digitLen) == 0) {
            count += digit;
            break;
        }
    }
    return count > 0 ? count : 1;
}

static Teban parse_mochigoma_owner(const char* line)
{
    return (strncmp(line, "後手", strlen("後手")) == 0) ? GOTE : SENTE;
}

static const char* find_mochigoma_list_start(const char* line)
{
    const char* zenkakuColon = strstr(line, "：");
    if (zenkakuColon)
        return zenkakuColon + strlen("：");

    const char* asciiColon = strchr(line, ':');
    return asciiColon ? asciiColon + 1 : NULL;
}

static void skip_spaces(const char** text)
{
    while (**text == ' ')
        (*text)++;
}

static bool match_koma_name(const char* text, Koma komaLimit, Koma* matchedKoma, size_t* matchedNameLen)
{
    for (Koma koma = 0; koma < komaLimit; koma++) {
        const char* komaName = tsume_koma_name(koma);
        size_t nameLen = strlen(komaName);
        if (nameLen > 0 && strncmp(text, komaName, nameLen) == 0) {
            *matchedKoma = koma;
            *matchedNameLen = nameLen;
            return true;
        }
    }

    *matchedKoma = NO_KOMA;
    *matchedNameLen = 0;
    return false;
}

static void copy_count_text(const char** text, char* countBuffer, size_t countBufferSize)
{
    size_t writeIndex = 0;
    while (!is_delimiter(**text) && writeIndex + 1 < countBufferSize) {
        countBuffer[writeIndex++] = **text;
        (*text)++;
    }
    countBuffer[writeIndex] = '\0';
}

static void skip_current_token(const char** text)
{
    while (!is_delimiter(**text))
        (*text)++;
}

static void parse_mochigoma_line(Board* board, const char* line)
{
    Teban owner = parse_mochigoma_owner(line);
    const char* currentText = find_mochigoma_list_start(line);
    if (!currentText)
        return;

    while (*currentText != '\0' && *currentText != '\n') {
        skip_spaces(&currentText);
        if (*currentText == '\0' || *currentText == '\n')
            break;

        const size_t nashiLen = strlen("なし");
        if (strncmp(currentText, "なし", nashiLen) == 0)
            break;

        Koma matchedKoma = NO_KOMA;
        size_t nameLen = 0;
        if (match_koma_name(currentText, MOCHIGOMA_LIMIT, &matchedKoma, &nameLen)) {
            currentText += nameLen;
            char countBuffer[16] = { 0 };
            copy_count_text(&currentText, countBuffer, sizeof(countBuffer));
            board->mochigoma[owner][matchedKoma] += (uint8_t)parse_count(countBuffer);
        } else {
            skip_current_token(&currentText);
        }
    }
}

static const char* find_board_rows_start(const char* text)
{
    const char* border = strstr(text, "+---");
    if (!border)
        return NULL;

    const char* firstRow = strchr(border, '\n');
    return firstRow ? firstRow + 1 : NULL;
}

static void parse_board_square(Board* board, int row, int col, const char** rowText)
{
    const int boardIndex = tsume_square_index(row, col);
    const size_t emptySquareLen = strlen("・");

    skip_spaces(rowText);

    Teban side = SENTE;
    if (**rowText == 'v') {
        side = GOTE;
        (*rowText)++;
    }

    if (strncmp(*rowText, "・", emptySquareLen) == 0) {
        *rowText += emptySquareLen;
        return;
    }

    Koma matchedKoma = NO_KOMA;
    size_t nameLen = 0;
    if (match_koma_name(*rowText, KOMA_KIND_COUNT, &matchedKoma, &nameLen)) {
        tsume_board_set_piece(board, boardIndex, matchedKoma, side);
        *rowText += nameLen;
        return;
    }

    *rowText += utf8_byte_len((unsigned char)**rowText);
}

static bool parse_board(Board* board, const char* text)
{
    const char* currentRow = find_board_rows_start(text);
    if (!currentRow)
        return false;

    for (int row = 0; row < BOARD_SIZE; row++) {
        currentRow = strchr(currentRow, '|');
        if (!currentRow)
            return false;
        currentRow++;

        for (int col = 0; col < BOARD_SIZE; col++)
            parse_board_square(board, row, col, &currentRow);

        currentRow = strchr(currentRow, '\n');
        if (!currentRow && row != BOARD_SIZE - 1)
            return false;
        if (currentRow)
            currentRow++;
    }
    return true;
}

static bool line_contains(const char* line, size_t lineLen, const char* needle)
{
    const char* match = strstr(line, needle);
    return match && match < line + lineLen;
}

TsumeParseResult tsume_parse_board_text(const char* input, Board* board)
{
    TsumeParseResult result = { TSUME_OK, "" };
    if (!input || !board) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "input and board are required");
        return result;
    }

    tsume_board_init(board);
    const char* currentLine = input;
    bool boardParsed = false;

    while (currentLine && *currentLine != '\0') {
        const char* nextLine = strchr(currentLine, '\n');
        size_t lineLen = nextLine ? (size_t)(nextLine - currentLine) : strlen(currentLine);

        if (line_contains(currentLine, lineLen, "持駒")) {
            parse_mochigoma_line(board, currentLine);
        } else if (!boardParsed && (line_contains(currentLine, lineLen, "９ ８ ７") || line_contains(currentLine, lineLen, "+---"))) {
            boardParsed = parse_board(board, currentLine);
        }

        currentLine = nextLine ? nextLine + 1 : NULL;
    }

    if (!boardParsed) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "could not find a complete 9x9 board");
    }
    return result;
}
