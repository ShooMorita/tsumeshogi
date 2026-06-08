#include "tsume_shogi.h"
#include "memory_arena.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    DFPN_UNKNOWN,
    DFPN_PROVEN,
    DFPN_DISPROVEN,
} DfpnState;

typedef struct {
    uint64_t key;
    bool used;
    bool hasProven;
    bool hasDisproven;
    bool hasBestMove;
    int provenPly;
    int disprovenPly;
    Move bestMove;
} DfpnEntry;

typedef struct {
    DfpnEntry entries[DFPN_TABLE_SIZE];
    uint64_t nodes;
    MemoryArena* arena;
} DfpnContext;

typedef enum {
    MOVE_ORDER_ATTACKER,
    MOVE_ORDER_DEFENDER,
} MoveOrderMode;

static int int_abs(int value)
{
    return value < 0 ? -value : value;
}

static int piece_order_value(Koma piece)
{
    switch (piece) {
    case FU:
        return 100;
    case KYO:
    case KEI:
        return 300;
    case GIN:
        return 500;
    case KIN:
    case TO:
    case NARIKYO:
    case NARIKEI:
    case NARIGIN:
        return 600;
    case KAKU:
        return 800;
    case UMA:
        return 900;
    case HISHA:
        return 1000;
    case RYU:
        return 1100;
    case GYOKU:
    case OU:
        return 2000;
    case NO_KOMA:
        return 0;
    }
    return 0;
}

static int find_side_king(const Board* board, Teban side)
{
    for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
        Koma piece = board->squares[square];
        switch (piece) {
        case GYOKU:
        case OU:
            if (tsume_board_square_side(board, square) == side)
                return square;
            break;
        case FU:
        case KYO:
        case KEI:
        case GIN:
        case KIN:
        case KAKU:
        case HISHA:
        case TO:
        case NARIKYO:
        case NARIKEI:
        case NARIGIN:
        case UMA:
        case RYU:
        case NO_KOMA:
            break;
        }
    }
    return -1;
}

static int square_distance(int from, int to)
{
    if (from < 0 || to < 0)
        return BOARD_SIZE * 2;
    return int_abs(tsume_square_row(from) - tsume_square_row(to)) + int_abs(tsume_square_col(from) - tsume_square_col(to));
}

static int move_order_score(const Move* move, int kingSquare, MoveOrderMode mode)
{
    int score = 0;
    switch (mode) {
    case MOVE_ORDER_ATTACKER:
        score += (BOARD_SIZE * 2 - square_distance(move->to, kingSquare)) * 10;
        if (move->captured != NO_KOMA)
            score += 10000 + piece_order_value(move->captured);
        switch (move->kind) {
        case MOVE_NORMAL:
            break;
        case MOVE_PROMOTE:
            score += 1500 + piece_order_value(tsume_promote(move->piece)) - piece_order_value(move->piece);
            break;
        case MOVE_DROP:
            score += 700 + piece_order_value(move->piece);
            break;
        }
        break;
    case MOVE_ORDER_DEFENDER:
        if (move->from == kingSquare)
            score += 8000;
        if (move->captured != NO_KOMA)
            score += 5000 + piece_order_value(move->captured);
        switch (move->kind) {
        case MOVE_NORMAL:
            break;
        case MOVE_PROMOTE:
            score += 300;
            break;
        case MOVE_DROP:
            score -= 200;
            break;
        }
        break;
    }
    return score;
}

static void order_moves(const Board* board, Teban side, MoveList* moves, MoveOrderMode mode)
{
    int scores[MAX_MOVES];
    int kingSquare = -1;
    switch (mode) {
    case MOVE_ORDER_ATTACKER:
        kingSquare = find_side_king(board, tsume_opponent(side));
        break;
    case MOVE_ORDER_DEFENDER:
        kingSquare = find_side_king(board, side);
        break;
    }
    for (int i = 0; i < moves->count; i++)
        scores[i] = move_order_score(&moves->moves[i], kingSquare, mode);

    for (int i = 1; i < moves->count; i++) {
        Move move = moves->moves[i];
        int score = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < score) {
            moves->moves[j + 1] = moves->moves[j];
            scores[j + 1] = scores[j];
            j--;
        }
        moves->moves[j + 1] = move;
        scores[j + 1] = score;
    }
}

static DfpnEntry* table_lookup(DfpnContext* context, uint64_t key)
{
    DfpnEntry* entry = &context->entries[key % DFPN_TABLE_SIZE];
    return (entry->used && entry->key == key) ? entry : NULL;
}

static DfpnEntry* table_entry_for_store(DfpnContext* context, uint64_t key)
{
    DfpnEntry* entry = &context->entries[key % DFPN_TABLE_SIZE];
    if (!entry->used || entry->key != key) {
        memset(entry, 0, sizeof(*entry));
        entry->key = key;
        entry->used = true;
    }
    return entry;
}

static void table_store_proven(DfpnContext* context, uint64_t key, const TsumeLine* line)
{
    DfpnEntry* entry = table_entry_for_store(context, key);
    if (entry->hasProven && entry->provenPly <= line->count)
        return;
    entry->hasProven = true;
    entry->provenPly = line->count;
    entry->hasBestMove = line->count > 0;
    if (entry->hasBestMove)
        entry->bestMove = line->moves[0];
}

static void table_store_disproven(DfpnContext* context, uint64_t key, int remainingPly)
{
    DfpnEntry* entry = table_entry_for_store(context, key);
    if (!entry->hasDisproven || remainingPly > entry->disprovenPly) {
        entry->hasDisproven = true;
        entry->disprovenPly = remainingPly;
    }
}

static uint64_t table_position_key(const Board* board, Teban side)
{
    return tsume_position_hash(board, side, 0);
}

static bool rebuild_cached_line(DfpnContext* context, const Board* board, Teban side, int remainingPly, TsumeLine* line)
{
    Board current = *board;
    Teban currentSide = side;
    int currentRemainingPly = remainingPly;
    line->count = 0;

    while (currentRemainingPly >= 0) {
        DfpnEntry* entry = table_lookup(context, table_position_key(&current, currentSide));
        if (!entry || !entry->hasProven || entry->provenPly > currentRemainingPly)
            return false;
        if (entry->provenPly == 0)
            return true;
        if (!entry->hasBestMove || line->count >= MAX_SOLUTION_MOVES)
            return false;

        Move move = entry->bestMove;
        line->moves[line->count++] = move;
        current = tsume_board_after_move(&current, &move);
        currentSide = tsume_opponent(currentSide);
        currentRemainingPly--;
    }

    return false;
}

static DfpnState dfpn_search(DfpnContext* context, const Board* board, Teban side, int remainingPly, TsumeLine* line);

static DfpnState solve_attacker_node(DfpnContext* context, const Board* board, int remainingPly, TsumeLine* line)
{
    ArenaMark mark = arena_mark(context->arena);
    MoveList* checkingMoves = (MoveList*)arena_alloc_zero(context->arena, sizeof(*checkingMoves));
    MoveList* checkingScratch = (MoveList*)arena_alloc_zero(context->arena, sizeof(*checkingScratch));
    TsumeLine* childLine = (TsumeLine*)arena_alloc_zero(context->arena, sizeof(*childLine));
    if (!checkingMoves || !checkingScratch || !childLine) {
        arena_rewind(context->arena, mark);
        return DFPN_DISPROVEN;
    }

    tsume_generate_legal_moves_with_scratch(board, SENTE, true, checkingMoves, checkingScratch);
    if (checkingMoves->count == 0) {
        arena_rewind(context->arena, mark);
        return DFPN_DISPROVEN;
    }
    order_moves(board, SENTE, checkingMoves, MOVE_ORDER_ATTACKER);

    for (int i = 0; i < checkingMoves->count; i++) {
        Move move = checkingMoves->moves[i];
        Board next = tsume_board_after_move(board, &move);

        childLine->count = 0;
        DfpnState childState = dfpn_search(context, &next, GOTE, remainingPly - 1, childLine);
        if (childState == DFPN_PROVEN) {
            line->count = 0;
            if (line->count < MAX_SOLUTION_MOVES)
                line->moves[line->count++] = move;
            for (int childIndex = 0; childIndex < childLine->count && line->count < MAX_SOLUTION_MOVES; childIndex++)
                line->moves[line->count++] = childLine->moves[childIndex];
            arena_rewind(context->arena, mark);
            return DFPN_PROVEN;
        }
    }
    arena_rewind(context->arena, mark);
    return DFPN_DISPROVEN;
}

static DfpnState solve_defender_node(DfpnContext* context, const Board* board, int remainingPly, TsumeLine* line)
{
    ArenaMark mark = arena_mark(context->arena);
    MoveList* defenderMoves = (MoveList*)arena_alloc_zero(context->arena, sizeof(*defenderMoves));
    MoveList* defenderScratch = (MoveList*)arena_alloc_zero(context->arena, sizeof(*defenderScratch));
    TsumeLine* bestLine = (TsumeLine*)arena_alloc_zero(context->arena, sizeof(*bestLine));
    TsumeLine* childLine = (TsumeLine*)arena_alloc_zero(context->arena, sizeof(*childLine));
    if (!defenderMoves || !defenderScratch || !bestLine || !childLine) {
        arena_rewind(context->arena, mark);
        return DFPN_DISPROVEN;
    }

    tsume_generate_legal_moves_with_scratch(board, GOTE, false, defenderMoves, defenderScratch);
    if (defenderMoves->count == 0) {
        arena_rewind(context->arena, mark);
        return DFPN_PROVEN;
    }
    order_moves(board, GOTE, defenderMoves, MOVE_ORDER_DEFENDER);

    for (int i = 0; i < defenderMoves->count; i++) {
        Move move = defenderMoves->moves[i];
        Board next = tsume_board_after_move(board, &move);

        childLine->count = 0;
        DfpnState childState = dfpn_search(context, &next, SENTE, remainingPly - 1, childLine);
        if (childState != DFPN_PROVEN) {
            arena_rewind(context->arena, mark);
            return DFPN_DISPROVEN;
        }

        if (bestLine->count == 0) {
            bestLine->moves[bestLine->count++] = move;
            for (int childIndex = 0; childIndex < childLine->count && bestLine->count < MAX_SOLUTION_MOVES; childIndex++)
                bestLine->moves[bestLine->count++] = childLine->moves[childIndex];
        }
    }

    *line = *bestLine;
    arena_rewind(context->arena, mark);
    return DFPN_PROVEN;
}

static DfpnState dfpn_search(DfpnContext* context, const Board* board, Teban side, int remainingPly, TsumeLine* line)
{
    context->nodes++;
    line->count = 0;

    if (remainingPly < 0)
        return DFPN_DISPROVEN;

    uint64_t key = table_position_key(board, side);
    DfpnEntry* cached = table_lookup(context, key);
    if (cached) {
        if (cached->hasProven && cached->provenPly <= remainingPly && rebuild_cached_line(context, board, side, remainingPly, line))
            return DFPN_PROVEN;
        if (cached->hasDisproven && remainingPly <= cached->disprovenPly)
            return DFPN_DISPROVEN;
    }

    DfpnState state = DFPN_DISPROVEN;
    switch (side) {
    case SENTE:
        state = solve_attacker_node(context, board, remainingPly, line);
        break;
    case GOTE:
        state = solve_defender_node(context, board, remainingPly, line);
        break;
    case TEBAN_COUNT:
        break;
    }

    switch (state) {
    case DFPN_PROVEN:
        table_store_proven(context, key, line);
        break;
    case DFPN_UNKNOWN:
    case DFPN_DISPROVEN:
        table_store_disproven(context, key, remainingPly);
        break;
    }
    return state;
}

TsumeSolveResult tsume_solve_dfpn(const Board* board, int maxPly, TsumeLine* line)
{
    TsumeSolveResult result = { TSUME_OK, maxPly, 0, "" };
    if (!board || !line) {
        result.status = TSUME_PARSE_ERROR;
        snprintf(result.message, sizeof(result.message), "board and line are required");
        return result;
    }
    if (maxPly <= 0 || maxPly % 2 == 0 || maxPly > MAX_SOLUTION_MOVES) {
        result.status = TSUME_INVALID_DEPTH;
        snprintf(result.message, sizeof(result.message), "max ply must be a positive odd number no larger than %d", MAX_SOLUTION_MOVES);
        return result;
    }

    MemoryArena arena;
    if (!arena_init(&arena, 1024 * 1024)) {
        result.status = TSUME_TIMEOUT;
        snprintf(result.message, sizeof(result.message), "could not allocate solver arena");
        return result;
    }

    DfpnContext* context = (DfpnContext*)arena_alloc_zero(&arena, sizeof(*context));
    if (!context) {
        arena_destroy(&arena);
        result.status = TSUME_TIMEOUT;
        snprintf(result.message, sizeof(result.message), "could not allocate solver context");
        return result;
    }
    context->arena = &arena;
    line->count = 0;

    DfpnState state = dfpn_search(context, board, SENTE, maxPly, line);
    result.nodes = context->nodes;
    arena_destroy(&arena);
    switch (state) {
    case DFPN_PROVEN:
        result.status = TSUME_OK;
        snprintf(result.message, sizeof(result.message), "mate found");
        break;
    case DFPN_UNKNOWN:
    case DFPN_DISPROVEN:
        result.status = TSUME_NO_MATE;
        snprintf(result.message, sizeof(result.message), "no mate found within %d ply", maxPly);
        break;
    }
    return result;
}
