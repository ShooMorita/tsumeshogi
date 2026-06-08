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

static char* error_json(TsumeStatus status, const char* message)
{
    char error[512];
    snprintf(error, sizeof(error), "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}", status_code(status), message);
    return duplicate_json(error);
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

static const char* file_label_from_col(int col)
{
    static const char* labels[BOARD_SIZE] = { "９", "８", "７", "６", "５", "４", "３", "２", "１" };
    return (0 <= col && col < BOARD_SIZE) ? labels[col] : "";
}

static const char* rank_label_from_row(int row)
{
    static const char* labels[BOARD_SIZE] = { "一", "二", "三", "四", "五", "六", "七", "八", "九" };
    return (0 <= row && row < BOARD_SIZE) ? labels[row] : "";
}

static const char* move_piece_label(Koma piece)
{
    switch (piece) {
    case NARIKYO:
        return "成香";
    case NARIKEI:
        return "成桂";
    case NARIGIN:
        return "成銀";
    case FU:
    case KYO:
    case KEI:
    case GIN:
    case KIN:
    case KAKU:
    case HISHA:
    case GYOKU:
    case OU:
    case TO:
    case UMA:
    case RYU:
    case NO_KOMA:
        return tsume_koma_name(piece);
    }
    return tsume_koma_name(piece);
}

static void append_move_json(char* buffer, size_t bufferSize, size_t* offset, const Move* move)
{
    const char* dropSuffix = "";
    const char* promoteSuffix = "";
    append_format(buffer, bufferSize, offset,
        "{\"piece\":\"%s\",\"side\":\"%s\",\"kind\":\"%s\",\"from\":",
        tsume_koma_code(move->piece),
        tsume_teban_name(move->side),
        move_kind_code(move->kind));

    switch (move->kind) {
    case MOVE_NORMAL:
        append_format(buffer, bufferSize, offset, "{\"row\":%d,\"col\":%d}", tsume_square_row(move->from), tsume_square_col(move->from));
        break;
    case MOVE_PROMOTE:
        append_format(buffer, bufferSize, offset, "{\"row\":%d,\"col\":%d}", tsume_square_row(move->from), tsume_square_col(move->from));
        promoteSuffix = "成";
        break;
    case MOVE_DROP:
        append_text(buffer, bufferSize, offset, "null");
        dropSuffix = "打";
        break;
    }

    append_format(buffer, bufferSize, offset,
        ",\"to\":{\"row\":%d,\"col\":%d},\"display\":\"%s%s%s%s%s\"}",
        tsume_square_row(move->to),
        tsume_square_col(move->to),
        file_label_from_col(tsume_square_col(move->to)),
        rank_label_from_row(tsume_square_row(move->to)),
        move_piece_label(move->piece),
        dropSuffix,
        promoteSuffix);
}

static void append_move_frames_json(char* buffer, size_t bufferSize, size_t* offset, const Board* initialBoard, const Move* moves, int moveCount)
{
    Board currentBoard = *initialBoard;

    append_text(buffer, bufferSize, offset, "\"frames\":[");
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];
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

static void append_frames_json(char* buffer, size_t bufferSize, size_t* offset, const Board* initialBoard, const TsumeLine* line)
{
    append_move_frames_json(buffer, bufferSize, offset, initialBoard, line->moves, line->count);
}

static Board board_after_replay_ply(const TsumeParseGameResult* game, int replayPly)
{
    Board board = game->initialBoard;
    for (int i = 0; i < replayPly; i++)
        board = tsume_board_after_move(&board, &game->moves[i]);
    return board;
}

char* tsume_replay_json(const char* input)
{
    TsumeParseGameResult parseResult = tsume_parse_game_text_value(input);
    if (parseResult.status != TSUME_OK)
        return error_json(parseResult.status, parseResult.message);

    size_t bufferSize = 65536 + (size_t)parseResult.moveCount * 8192;
    char* buffer = (char*)malloc(bufferSize);
    if (!buffer)
        return NULL;

    size_t offset = 0;
    append_text(buffer, bufferSize, &offset, "{\"ok\":true,");
    append_board_json(buffer, bufferSize, &offset, &parseResult.initialBoard);
    append_text(buffer, bufferSize, &offset, ",\"replay\":{\"moveCount\":");
    append_format(buffer, bufferSize, &offset, "%d,\"moves\":[", parseResult.moveCount);
    for (int i = 0; i < parseResult.moveCount; i++) {
        if (i > 0)
            append_text(buffer, bufferSize, &offset, ",");
        append_move_json(buffer, bufferSize, &offset, &parseResult.moves[i]);
    }
    append_text(buffer, bufferSize, &offset, "]},");
    append_move_frames_json(buffer, bufferSize, &offset, &parseResult.initialBoard, parseResult.moves, parseResult.moveCount);
    append_text(buffer, bufferSize, &offset, "}");
    buffer[bufferSize - 1] = '\0';
    return buffer;
}

static char* solve_parsed_game_at_json(const TsumeParseGameResult* parseResult, int maxPly, int replayPly)
{
    if (replayPly < 0 || replayPly > parseResult->moveCount)
        return error_json(TSUME_PARSE_ERROR, "replay ply is out of range");

    Board board = board_after_replay_ply(parseResult, replayPly);

    TsumeLine line = { 0 };
    TsumeSolveResult solveResult = tsume_solve_dfpn(&board, maxPly, &line);

    size_t bufferSize = 65536 + (size_t)line.count * 8192;
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
    append_format(buffer, bufferSize, &offset, ",\"replayPly\":%d", replayPly);
    append_text(buffer, bufferSize, &offset, ",");
    append_frames_json(buffer, bufferSize, &offset, &board, &line);
    append_text(buffer, bufferSize, &offset, "}");
    buffer[bufferSize - 1] = '\0';
    return buffer;
}

char* tsume_solve_at_json(const char* input, int maxPly, int replayPly)
{
    TsumeParseGameResult parseResult = tsume_parse_game_text_value(input);
    if (parseResult.status != TSUME_OK)
        return error_json(parseResult.status, parseResult.message);

    return solve_parsed_game_at_json(&parseResult, maxPly, replayPly);
}

char* tsume_solve_json(const char* input, int maxPly)
{
    TsumeParseGameResult parseResult = tsume_parse_game_text_value(input);
    if (parseResult.status != TSUME_OK)
        return error_json(parseResult.status, parseResult.message);

    return solve_parsed_game_at_json(&parseResult, maxPly, parseResult.moveCount);
}

void tsume_free(char* ptr)
{
    free(ptr);
}
