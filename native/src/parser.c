#include "tsume_shogi.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    TsumeStatus status;
    char message[128];
} ParseResult;

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

static int parse_count(const char* countText)
{
    if (!countText || is_delimiter(*countText))
        return 1;
    if ('0' <= countText[0] && countText[0] <= '9')
        return countText[0] - '0';

    int count = 0;
    const size_t juLen = strlen("十");
    if (strncmp(countText, "十", juLen) == 0) {
        count = 10;
        countText += juLen;
    }
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

static const char* find_mochigoma_list_start(const char* line, size_t lineLen)
{
    const char* lineEnd = line + lineLen;
    const size_t zenkakuColonLen = strlen("：");

    for (const char* current = line; current < lineEnd; current++) {
        if (*current == ':')
            return current + 1;
        if (current + zenkakuColonLen <= lineEnd && strncmp(current, "：", zenkakuColonLen) == 0)
            return current + zenkakuColonLen;
    }
    return NULL;
}

static const char* skip_spaces(const char* text)
{
    const size_t zenkakuSpaceLen = strlen("　");
    while (*text == ' ' || *text == '\t' || strncmp(text, "　", zenkakuSpaceLen) == 0)
        text += (*text == ' ' || *text == '\t') ? 1 : zenkakuSpaceLen;
    return text;
}

static bool starts_with_text(const char* text, const char* prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

typedef struct {
    Koma koma;
    size_t nameLen;
    bool matched;
} KomaMatch;

static KomaMatch match_koma_name(const char* text, Koma komaLimit)
{
    for (Koma koma = 0; koma < komaLimit; koma++) {
        const char* komaName = tsume_koma_name(koma);
        size_t nameLen = strlen(komaName);
        if (nameLen > 0 && strncmp(text, komaName, nameLen) == 0)
            return (KomaMatch) { koma, nameLen, true };
    }

    return (KomaMatch) { NO_KOMA, 0, false };
}

static const char* skip_current_token(const char* text)
{
    while (!is_delimiter(*text))
        text++;
    return text;
}

typedef struct {
    Koma koma;
    int count;
    const char* nextText;
    bool matched;
} MochigomaToken;

static MochigomaToken parse_mochigoma_token(const char* text)
{
    const char* tokenStart = skip_spaces(text);
    KomaMatch match = match_koma_name(tokenStart, MOCHIGOMA_LIMIT);
    if (!match.matched)
        return (MochigomaToken) { NO_KOMA, 0, skip_current_token(tokenStart), false };

    const char* countText = tokenStart + match.nameLen;
    return (MochigomaToken) {
        match.koma,
        parse_count(countText),
        skip_current_token(countText),
        true,
    };
}

static void parse_mochigoma_line(Board* board, const char* line, size_t lineLen)
{
    Teban owner = parse_mochigoma_owner(line);
    const char* currentText = find_mochigoma_list_start(line, lineLen);
    if (!currentText)
        return;

    const char* lineEnd = line + lineLen;
    while (currentText < lineEnd && *currentText != '\0' && *currentText != '\n') {
        currentText = skip_spaces(currentText);
        if (currentText >= lineEnd || *currentText == '\0' || *currentText == '\n')
            break;

        const size_t nashiLen = strlen("なし");
        if (strncmp(currentText, "なし", nashiLen) == 0)
            break;

        MochigomaToken token = parse_mochigoma_token(currentText);
        if (token.matched)
            board->mochigoma[owner][token.koma] += (uint8_t)token.count;
        currentText = token.nextText;
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

typedef struct {
    Koma koma;
    Teban side;
    const char* nextText;
} BoardSquareToken;

static BoardSquareToken parse_board_square(const char* rowText)
{
    const size_t emptySquareLen = strlen("・");
    const char* currentText = skip_spaces(rowText);

    Teban side = SENTE;
    if (*currentText == 'v') {
        side = GOTE;
        currentText++;
    }

    if (strncmp(currentText, "・", emptySquareLen) == 0)
        return (BoardSquareToken) { NO_KOMA, side, currentText + emptySquareLen };

    KomaMatch match = match_koma_name(currentText, KOMA_KIND_COUNT);
    if (match.matched)
        return (BoardSquareToken) { match.koma, side, currentText + match.nameLen };

    return (BoardSquareToken) { NO_KOMA, side, currentText + utf8_byte_len((unsigned char)*currentText) };
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

        for (int col = 0; col < BOARD_SIZE; col++) {
            BoardSquareToken token = parse_board_square(currentRow);
            if (token.koma != NO_KOMA)
                tsume_board_set_piece(board, tsume_square_index(row, col), token.koma, token.side);
            currentRow = token.nextText;
        }

        currentRow = strchr(currentRow, '\n');
        if (!currentRow && row != BOARD_SIZE - 1)
            return false;
        if (currentRow)
            currentRow++;
    }
    return true;
}

static void setup_standard_board(Board* board)
{
    tsume_board_init(board);

    Koma backRank[BOARD_SIZE] = { KYO, KEI, GIN, KIN, GYOKU, KIN, GIN, KEI, KYO };
    for (int col = 0; col < BOARD_SIZE; col++) {
        tsume_board_set_piece(board, tsume_square_index(0, col), backRank[col], GOTE);
        tsume_board_set_piece(board, tsume_square_index(2, col), FU, GOTE);
        tsume_board_set_piece(board, tsume_square_index(6, col), FU, SENTE);
        tsume_board_set_piece(board, tsume_square_index(8, col), backRank[col], SENTE);
    }

    tsume_board_set_piece(board, tsume_square_index(1, 1), HISHA, GOTE);
    tsume_board_set_piece(board, tsume_square_index(1, 7), KAKU, GOTE);
    tsume_board_set_piece(board, tsume_square_index(7, 1), KAKU, SENTE);
    tsume_board_set_piece(board, tsume_square_index(7, 7), HISHA, SENTE);
}

typedef struct {
    int value;
    size_t len;
    bool matched;
} DigitMatch;

static DigitMatch match_digit(const char* text)
{
    if ('1' <= *text && *text <= '9')
        return (DigitMatch) { *text - '0', 1, true };

    const char* zenkakuDigits[] = { "", "１", "２", "３", "４", "５", "６", "７", "８", "９" };
    const char* kanjiDigits[] = { "", "一", "二", "三", "四", "五", "六", "七", "八", "九" };
    for (int value = 1; value <= 9; value++) {
        size_t zenkakuLen = strlen(zenkakuDigits[value]);
        if (strncmp(text, zenkakuDigits[value], zenkakuLen) == 0)
            return (DigitMatch) { value, zenkakuLen, true };

        size_t kanjiLen = strlen(kanjiDigits[value]);
        if (strncmp(text, kanjiDigits[value], kanjiLen) == 0)
            return (DigitMatch) { value, kanjiLen, true };
    }

    return (DigitMatch) { 0, 0, false };
}

static bool parse_kif_square(const char* text, int* square, const char** nextText)
{
    DigitMatch file = match_digit(text);
    if (!file.matched)
        return false;

    DigitMatch rank = match_digit(text + file.len);
    if (!rank.matched)
        return false;

    *square = tsume_square_index(rank.value - 1, BOARD_SIZE - file.value);
    if (nextText)
        *nextText = text + file.len + rank.len;
    return true;
}

typedef struct {
    Koma koma;
    size_t nameLen;
    bool matched;
} KifPieceMatch;

typedef struct {
    const char* name;
    Koma koma;
} KifPieceAlias;

static KifPieceMatch match_kif_piece_name(const char* text)
{
    static const KifPieceAlias aliases[] = {
        { "成銀", NARIGIN },
        { "成桂", NARIKEI },
        { "成香", NARIKYO },
        { "歩", FU },
        { "香", KYO },
        { "桂", KEI },
        { "銀", GIN },
        { "金", KIN },
        { "角", KAKU },
        { "飛", HISHA },
        { "玉", GYOKU },
        { "王", OU },
        { "と", TO },
        { "杏", NARIKYO },
        { "圭", NARIKEI },
        { "全", NARIGIN },
        { "馬", UMA },
        { "龍", RYU },
        { "竜", RYU },
    };

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        size_t nameLen = strlen(aliases[i].name);
        if (strncmp(text, aliases[i].name, nameLen) == 0)
            return (KifPieceMatch) { aliases[i].koma, nameLen, true };
    }

    return (KifPieceMatch) { NO_KOMA, 0, false };
}

static bool king_names_match(Koma left, Koma right)
{
    return (left == GYOKU || left == OU) && (right == GYOKU || right == OU);
}

static bool kif_piece_matches_board(Koma notationPiece, Koma boardPiece)
{
    return notationPiece == boardPiece || king_names_match(notationPiece, boardPiece);
}

static bool parse_source_square_after_move_text(const char* text, int* square)
{
    const char* current = text;
    const size_t zenkakuOpenLen = strlen("（");

    while (*current != '\0' && *current != '\n' && *current != '\r') {
        if (*current == '(') {
            return parse_kif_square(current + 1, square, NULL);
        }
        if (strncmp(current, "（", zenkakuOpenLen) == 0) {
            return parse_kif_square(current + zenkakuOpenLen, square, NULL);
        }
        if (*current == ' ' || *current == '\t')
            return false;

        current += utf8_byte_len((unsigned char)*current);
    }

    return false;
}

static bool parse_move_number(const char* line, int* moveNumber, const char** moveText)
{
    const char* current = skip_spaces(line);
    if (*current < '0' || *current > '9')
        return false;

    int number = 0;
    while ('0' <= *current && *current <= '9') {
        number = number * 10 + (*current - '0');
        current++;
    }

    *moveNumber = number;
    *moveText = skip_spaces(current);
    return true;
}

typedef struct {
    TsumeStatus status;
    char message[128];
    Move move;
    int moveNumber;
    bool matched;
    bool terminal;
} KifMoveParseResult;

static bool is_terminal_kif_move(const char* text)
{
    return starts_with_text(text, "投了")
        || starts_with_text(text, "中断")
        || starts_with_text(text, "千日手")
        || starts_with_text(text, "持将棋")
        || starts_with_text(text, "切れ負け")
        || starts_with_text(text, "反則勝ち")
        || starts_with_text(text, "反則負け");
}

static KifMoveParseResult parse_kif_move_line(const Board* board, const char* line, int previousDestination)
{
    KifMoveParseResult result = { TSUME_OK, "", { 0 }, 0, false, false };

    int moveNumber = 0;
    const char* current = NULL;
    if (!parse_move_number(line, &moveNumber, &current))
        return result;

    result.matched = true;
    result.moveNumber = moveNumber;

    if (is_terminal_kif_move(current)) {
        result.terminal = true;
        return result;
    }

    int to = -1;
    const size_t sameLen = strlen("同");
    if (strncmp(current, "同", sameLen) == 0) {
        if (previousDestination < 0) {
            result.status = TSUME_PARSE_ERROR;
            snprintf(result.message, sizeof(result.message), "same-square move has no previous destination at move %d", moveNumber);
            return result;
        }
        to = previousDestination;
        current = skip_spaces(current + sameLen);
    } else if (!parse_kif_square(current, &to, &current)) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "could not parse destination at move %d", moveNumber);
        return result;
    }

    current = skip_spaces(current);
    KifPieceMatch pieceMatch = match_kif_piece_name(current);
    if (!pieceMatch.matched) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "could not parse piece at move %d", moveNumber);
        return result;
    }
    current += pieceMatch.nameLen;

    MoveKind kind = MOVE_NORMAL;
    if (starts_with_text(current, "不成")) {
        current += strlen("不成");
    } else if (starts_with_text(current, "成") && tsume_can_promote(pieceMatch.koma)) {
        kind = MOVE_PROMOTE;
        current += strlen("成");
    }

    if (starts_with_text(current, "打")) {
        kind = MOVE_DROP;
        current += strlen("打");
    }

    Teban side = (moveNumber % 2 == 1) ? SENTE : GOTE;
    Move move = {
        .piece = pieceMatch.koma,
        .captured = NO_KOMA,
        .side = side,
        .from = -1,
        .to = to,
        .kind = kind,
    };

    if (kind == MOVE_DROP) {
        if (pieceMatch.koma >= MOCHIGOMA_LIMIT || board->mochigoma[side][pieceMatch.koma] == 0) {
            result.status = TSUME_PARSE_ERROR;
            snprintf(result.message, sizeof(result.message), "piece is not available to drop at move %d", moveNumber);
            return result;
        }
        if (board->squares[to] != NO_KOMA) {
            result.status = TSUME_PARSE_ERROR;
            snprintf(result.message, sizeof(result.message), "drop target is occupied at move %d", moveNumber);
            return result;
        }
        result.move = move;
        return result;
    }

    int from = -1;
    if (!parse_source_square_after_move_text(current, &from)) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "could not parse source square at move %d", moveNumber);
        return result;
    }

    Koma boardPiece = board->squares[from];
    if (boardPiece == NO_KOMA || tsume_board_square_side(board, from) != side || !kif_piece_matches_board(pieceMatch.koma, boardPiece)) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "source square does not match move notation at move %d", moveNumber);
        return result;
    }

    Koma captured = board->squares[to];
    if (captured != NO_KOMA && tsume_board_square_side(board, to) == side) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "move captures own piece at move %d", moveNumber);
        return result;
    }

    if (kind == MOVE_PROMOTE && !tsume_can_promote(boardPiece)) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "piece cannot promote at move %d", moveNumber);
        return result;
    }

    move.piece = boardPiece;
    move.captured = captured;
    move.from = from;
    result.move = move;
    return result;
}

static bool line_contains(const char* line, size_t lineLen, const char* needle)
{
    size_t needleLen = strlen(needle);
    if (needleLen == 0 || needleLen > lineLen)
        return false;

    for (size_t offset = 0; offset + needleLen <= lineLen; offset++) {
        if (strncmp(line + offset, needle, needleLen) == 0)
            return true;
    }
    return false;
}

static ParseResult parse_board_text_into(const char* input, Board* board)
{
    ParseResult result = { TSUME_OK, "" };
    tsume_board_init(board);
    if (!input) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "input is required");
        return result;
    }

    const char* currentLine = input;
    bool boardParsed = false;

    while (currentLine && *currentLine != '\0') {
        const char* nextLine = strchr(currentLine, '\n');
        size_t lineLen = nextLine ? (size_t)(nextLine - currentLine) : strlen(currentLine);

        if (line_contains(currentLine, lineLen, "持駒")) {
            parse_mochigoma_line(board, currentLine, lineLen);
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

static ParseResult parse_kifu_text_into(const char* input, TsumeParseGameResult* game)
{
    ParseResult result = { TSUME_OK, "" };
    if (!input) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "input is required");
        return result;
    }

    setup_standard_board(&game->initialBoard);
    game->moveCount = 0;

    Board currentBoard = game->initialBoard;
    const char* currentLine = input;
    bool hasStandardStart = false;
    bool inMoveSection = false;
    bool sawMoveSection = false;
    int previousDestination = -1;

    while (currentLine && *currentLine != '\0') {
        const char* nextLine = strchr(currentLine, '\n');
        size_t lineLen = nextLine ? (size_t)(nextLine - currentLine) : strlen(currentLine);

        if (line_contains(currentLine, lineLen, "初期局面：通常") || line_contains(currentLine, lineLen, "手合割：平手"))
            hasStandardStart = true;

        if (!inMoveSection && line_contains(currentLine, lineLen, "手数") && line_contains(currentLine, lineLen, "指手")) {
            inMoveSection = true;
            sawMoveSection = true;
            currentLine = nextLine ? nextLine + 1 : NULL;
            continue;
        }

        if (inMoveSection) {
            KifMoveParseResult parsedMove = parse_kif_move_line(&currentBoard, currentLine, previousDestination);
            if (parsedMove.status != TSUME_OK) {
                result.status = parsedMove.status;
                snprintf(result.message, sizeof(result.message), "%s", parsedMove.message);
                return result;
            }
            if (parsedMove.terminal)
                break;
            if (parsedMove.matched) {
                if (game->moveCount >= MAX_KIFU_MOVES) {
                    result.status = TSUME_PARSE_ERROR;
                    snprintf(result.message, sizeof(result.message), "kifu has more than %d moves", MAX_KIFU_MOVES);
                    return result;
                }

                Move move = parsedMove.move;
                game->moves[game->moveCount++] = move;
                currentBoard = tsume_board_after_move(&currentBoard, &move);
                previousDestination = move.to;
            }
        }

        currentLine = nextLine ? nextLine + 1 : NULL;
    }

    if (!hasStandardStart && !sawMoveSection) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "could not find a complete 9x9 board or standard kifu move list");
        return result;
    }
    if (!hasStandardStart) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "only standard initial-position kifu is supported");
        return result;
    }

    return result;
}

TsumeParseGameResult tsume_parse_game_text_value(const char* input)
{
    TsumeParseGameResult result = { TSUME_OK, "", { 0 }, { 0 }, 0 };

    Board board;
    ParseResult parsedBoard = parse_board_text_into(input, &board);
    if (parsedBoard.status == TSUME_OK) {
        result.initialBoard = board;
        return result;
    }

    ParseResult parsedKifu = parse_kifu_text_into(input, &result);
    result.status = parsedKifu.status;
    snprintf(result.message, sizeof(result.message), "%s", parsedKifu.message);
    return result;
}

TsumeParseBoardResult tsume_parse_board_text_value(const char* input)
{
    TsumeParseBoardResult result = { TSUME_OK, "", { 0 } };
    TsumeParseGameResult parsed = tsume_parse_game_text_value(input);
    result.status = parsed.status;
    snprintf(result.message, sizeof(result.message), "%s", parsed.message);
    if (parsed.status != TSUME_OK)
        return result;

    Board board = parsed.initialBoard;
    for (int i = 0; i < parsed.moveCount; i++)
        board = tsume_board_after_move(&board, &parsed.moves[i]);
    result.board = board;
    return result;
}
