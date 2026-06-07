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
    int proof;
    int disproof;
    int remainingPly;
    bool used;
} DfpnEntry;

typedef struct {
    DfpnEntry entries[DFPN_TABLE_SIZE];
    uint64_t nodes;
    MemoryArena* arena;
} DfpnContext;

static DfpnEntry* table_lookup(DfpnContext* context, uint64_t key)
{
    DfpnEntry* entry = &context->entries[key % DFPN_TABLE_SIZE];
    return (entry->used && entry->key == key) ? entry : NULL;
}

static void table_store(DfpnContext* context, uint64_t key, int remainingPly, int proof, int disproof)
{
    DfpnEntry* entry = &context->entries[key % DFPN_TABLE_SIZE];
    entry->key = key;
    entry->remainingPly = remainingPly;
    entry->proof = proof;
    entry->disproof = disproof;
    entry->used = true;
}

static DfpnState dfpn_search(DfpnContext* context, const Board* board, Teban side, int remainingPly, TsumeLine* line);

static DfpnState solve_attacker_node(DfpnContext* context, const Board* board, int remainingPly, TsumeLine* line)
{
    ArenaMark mark = arena_mark(context->arena);
    MoveList* checkingMoves = (MoveList*)arena_alloc_zero(context->arena, sizeof(*checkingMoves));
    MoveList* checkingScratch = (MoveList*)arena_alloc_zero(context->arena, sizeof(*checkingScratch));
    MoveList* defenderMoves = (MoveList*)arena_alloc_zero(context->arena, sizeof(*defenderMoves));
    MoveList* defenderScratch = (MoveList*)arena_alloc_zero(context->arena, sizeof(*defenderScratch));
    TsumeLine* childLine = (TsumeLine*)arena_alloc_zero(context->arena, sizeof(*childLine));
    if (!checkingMoves || !checkingScratch || !defenderMoves || !defenderScratch || !childLine) {
        arena_rewind(context->arena, mark);
        return DFPN_DISPROVEN;
    }

    tsume_generate_legal_moves_with_scratch(board, SENTE, true, checkingMoves, checkingScratch);
    if (checkingMoves->count == 0) {
        arena_rewind(context->arena, mark);
        return DFPN_DISPROVEN;
    }

    for (int i = 0; i < checkingMoves->count; i++) {
        Board next = *board;
        Move move = checkingMoves->moves[i];
        tsume_apply_move(&next, &move);

        defenderMoves->count = 0;
        defenderScratch->count = 0;
        tsume_generate_legal_moves_with_scratch(&next, GOTE, false, defenderMoves, defenderScratch);
        if (defenderMoves->count == 0) {
            line->count = 1;
            line->moves[0] = move;
            arena_rewind(context->arena, mark);
            return DFPN_PROVEN;
        }

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

    for (int i = 0; i < defenderMoves->count; i++) {
        Board next = *board;
        Move move = defenderMoves->moves[i];
        tsume_apply_move(&next, &move);

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

    uint64_t key = tsume_position_hash(board, side, remainingPly);
    DfpnEntry* cached = table_lookup(context, key);
    if (cached && cached->remainingPly >= remainingPly) {
        if (cached->proof == 0)
            return DFPN_PROVEN;
        if (cached->disproof == 0)
            return DFPN_DISPROVEN;
    }

    DfpnState state = side == SENTE
        ? solve_attacker_node(context, board, remainingPly, line)
        : solve_defender_node(context, board, remainingPly, line);

    table_store(context, key, remainingPly, state == DFPN_PROVEN ? 0 : 1, state == DFPN_DISPROVEN ? 0 : 1);
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
    if (!arena_init(&arena, 2 * 1024 * 1024)) {
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
    if (state == DFPN_PROVEN) {
        result.status = TSUME_OK;
        snprintf(result.message, sizeof(result.message), "mate found");
    } else {
        result.status = TSUME_NO_MATE;
        snprintf(result.message, sizeof(result.message), "no mate found within %d ply", maxPly);
    }
    return result;
}
