#include "tsume_shogi.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static void append_text(char* buffer, size_t bufferSize, size_t* offset, const char* text)
{
    if (*offset + 1 >= bufferSize)
        return;
    int written = snprintf(buffer + *offset, bufferSize - *offset, "%s", text);
    if (written > 0) {
        size_t remaining = bufferSize - *offset;
        *offset += (size_t)written < remaining ? (size_t)written : remaining - 1;
    }
}

static void append_format(char* buffer, size_t bufferSize, size_t* offset, const char* format, ...)
{
    if (*offset + 1 >= bufferSize)
        return;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer + *offset, bufferSize - *offset, format, args);
    va_end(args);
    if (written > 0) {
        size_t remaining = bufferSize - *offset;
        *offset += (size_t)written < remaining ? (size_t)written : remaining - 1;
    }
}

static const char* status_code(TsumeStatus status)
{
    switch (status) {
    case TSUME_OK:
        return "ok";
    case TSUME_PARSE_ERROR:
        return "parse_error";
    case TSUME_INVALID_DEPTH:
        return "invalid_depth";
    case TSUME_NO_MATE:
        return "no_mate";
    case TSUME_TIMEOUT:
        return "timeout";
    }
    return "unknown";
}

static char* duplicate_json(const char* text)
{
    size_t len = strlen(text);
    char* out = (char*)malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, text, len + 1);
    return out;
}

static void append_board_value_json(char* buffer, size_t bufferSize, size_t* offset, const Board* board)
{
    append_text(buffer, bufferSize, offset, "{\"squares\":[");
    for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
        Koma koma = board->squares[square];
        append_format(buffer, bufferSize, offset,
            "%s{\"row\":%d,\"col\":%d,\"piece\":\"%s\",\"owner\":\"%s\"}",
            square == 0 ? "" : ",",
            tsume_square_row(square),
            tsume_square_col(square),
            tsume_koma_code(koma),
            koma == NO_KOMA ? "none" : tsume_teban_name(tsume_board_square_side(board, square)));
    }
    append_text(buffer, bufferSize, offset, "],\"hands\":{\"sente\":{");
    for (int koma = 0; koma < MOCHIGOMA_LIMIT; koma++)
        append_format(buffer, bufferSize, offset, "%s\"%s\":%d", koma == 0 ? "" : ",", tsume_koma_code((Koma)koma), board->mochigoma[SENTE][koma]);
    append_text(buffer, bufferSize, offset, "},\"gote\":{");
    for (int koma = 0; koma < MOCHIGOMA_LIMIT; koma++)
        append_format(buffer, bufferSize, offset, "%s\"%s\":%d", koma == 0 ? "" : ",", tsume_koma_code((Koma)koma), board->mochigoma[GOTE][koma]);
    append_text(buffer, bufferSize, offset, "}}}");
}

static void append_board_json(char* buffer, size_t bufferSize, size_t* offset, const Board* board)
{
    append_text(buffer, bufferSize, offset, "\"board\":");
    append_board_value_json(buffer, bufferSize, offset, board);
}

static const char* move_kind_code(MoveKind kind)
{
    switch (kind) {
    case MOVE_NORMAL:
        return "normal";
    case MOVE_PROMOTE:
        return "promote";
    case MOVE_DROP:
        return "drop";
    }
    return "normal";
}

static void append_move_json(char* buffer, size_t bufferSize, size_t* offset, const Move* move)
{
    append_format(buffer, bufferSize, offset,
        "{\"piece\":\"%s\",\"side\":\"%s\",\"kind\":\"%s\",\"from\":",
        tsume_koma_code(move->piece),
        tsume_teban_name(move->side),
        move_kind_code(move->kind));
    if (move->kind == MOVE_DROP) {
        append_text(buffer, bufferSize, offset, "null");
    } else {
        append_format(buffer, bufferSize, offset, "{\"row\":%d,\"col\":%d}", tsume_square_row(move->from), tsume_square_col(move->from));
    }
    append_format(buffer, bufferSize, offset,
        ",\"to\":{\"row\":%d,\"col\":%d},\"display\":\"%s%s%s\"}",
        tsume_square_row(move->to),
        tsume_square_col(move->to),
        tsume_koma_name(move->piece),
        move->kind == MOVE_DROP ? "打" : "",
        move->kind == MOVE_PROMOTE ? "成" : "");
}

static void append_frames_json(char* buffer, size_t bufferSize, size_t* offset, const Board* initialBoard, const TsumeLine* line)
{
    Board currentBoard = *initialBoard;

    append_text(buffer, bufferSize, offset, "\"frames\":[");
    for (int i = 0; i < line->count; i++) {
        Move move = line->moves[i];
        currentBoard = tsume_board_after_move(&currentBoard, &move);

        if (i > 0)
            append_text(buffer, bufferSize, offset, ",");

        append_text(buffer, bufferSize, offset, "{\"board\":");
        append_board_value_json(buffer, bufferSize, offset, &currentBoard);
        append_text(buffer, bufferSize, offset, ",\"lastMove\":");
        append_move_json(buffer, bufferSize, offset, &move);
        append_text(buffer, bufferSize, offset, "}");
    }
    append_text(buffer, bufferSize, offset, "]");
}

char* tsume_solve_json(const char* input, int maxPly)
{
    TsumeParseBoardResult parseResult = tsume_parse_board_text_value(input);
    if (parseResult.status != TSUME_OK) {
        char error[512];
        snprintf(error, sizeof(error), "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}", status_code(parseResult.status), parseResult.message);
        return duplicate_json(error);
    }
    Board board = parseResult.board;

    TsumeLine line = { 0 };
    TsumeSolveResult solveResult = tsume_solve_dfpn(&board, maxPly, &line);

    size_t bufferSize = 65536;
    char* buffer = (char*)malloc(bufferSize);
    if (!buffer)
        return NULL;
    size_t offset = 0;
    append_format(buffer, bufferSize, &offset,
        "{\"ok\":%s,\"solver\":{\"status\":\"%s\",\"maxPly\":%d,\"nodes\":%llu,\"message\":\"%s\",\"moves\":[",
        solveResult.status == TSUME_OK ? "true" : "false",
        status_code(solveResult.status),
        solveResult.maxPly,
        (unsigned long long)solveResult.nodes,
        solveResult.message);
    for (int i = 0; i < line.count; i++) {
        if (i > 0)
            append_text(buffer, bufferSize, &offset, ",");
        append_move_json(buffer, bufferSize, &offset, &line.moves[i]);
    }
    append_text(buffer, bufferSize, &offset, "]},");
    append_board_json(buffer, bufferSize, &offset, &board);
    append_text(buffer, bufferSize, &offset, ",");
    append_frames_json(buffer, bufferSize, &offset, &board, &line);
    append_text(buffer, bufferSize, &offset, "}");
    buffer[bufferSize - 1] = '\0';
    return buffer;
}

void tsume_free(char* ptr)
{
    free(ptr);
}
