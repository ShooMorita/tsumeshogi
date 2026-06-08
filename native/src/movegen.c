#include "tsume_shogi.h"

#include <stdlib.h>
#include <string.h>

static bool in_board(int row, int col)
{
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

static bool is_promotion_zone(Teban side, int row)
{
    switch (side) {
    case SENTE:
        return row <= 2;
    case GOTE:
        return row >= 6;
    case TEBAN_COUNT:
        break;
    }
    return false;
}

static int forward_direction(Teban side)
{
    switch (side) {
    case SENTE:
        return -1;
    case GOTE:
        return 1;
    case TEBAN_COUNT:
        break;
    }
    return 0;
}

static bool must_promote(Teban side, Koma koma, int toRow)
{
    switch (side) {
    case SENTE:
        return (koma == FU || koma == KYO) ? toRow == 0 : (koma == KEI && toRow <= 1);
    case GOTE:
        return (koma == FU || koma == KYO) ? toRow == 8 : (koma == KEI && toRow >= 7);
    case TEBAN_COUNT:
        break;
    }
    return false;
}

static bool can_drop_on_row(Teban side, Koma koma, int row)
{
    switch (side) {
    case SENTE:
        if ((koma == FU || koma == KYO) && row == 0)
            return false;
        if (koma == KEI && row <= 1)
            return false;
        break;
    case GOTE:
        if ((koma == FU || koma == KYO) && row == 8)
            return false;
        if (koma == KEI && row >= 7)
            return false;
        break;
    case TEBAN_COUNT:
        return false;
    }
    return true;
}

static bool file_has_unpromoted_pawn(const Board* board, Teban side, int col)
{
    for (int row = 0; row < BOARD_SIZE; row++) {
        int square = tsume_square_index(row, col);
        if (board->squares[square] == FU && tsume_board_square_side(board, square) == side)
            return true;
    }
    return false;
}

static bool path_is_clear(const Board* board, int fromRow, int fromCol, int targetRow, int targetCol, int dr, int dc)
{
    int row = fromRow + dr;
    int col = fromCol + dc;
    while (row != targetRow || col != targetCol) {
        if (!in_board(row, col))
            return false;
        if (board->squares[tsume_square_index(row, col)] != NO_KOMA)
            return false;
        row += dr;
        col += dc;
    }
    return true;
}

static void add_move(MoveList* list, const Move* move)
{
    if (list->count < MAX_MOVES)
        list->moves[list->count++] = *move;
}

static void add_step_move(const Board* board, Teban side, int from, int to, MoveList* list)
{
    if (to < 0 || to >= BOARD_SQUARE_COUNT)
        return;
    Koma target = board->squares[to];
    if (target != NO_KOMA && tsume_board_square_side(board, to) == side)
        return;

    Move move = {
        .piece = board->squares[from],
        .captured = target,
        .side = side,
        .from = from,
        .to = to,
        .kind = MOVE_NORMAL,
    };
    int fromRow = tsume_square_row(from);
    int toRow = tsume_square_row(to);
    if (tsume_can_promote(move.piece) && (is_promotion_zone(side, fromRow) || is_promotion_zone(side, toRow))) {
        if (must_promote(side, move.piece, toRow)) {
            move.kind = MOVE_PROMOTE;
            add_move(list, &move);
        } else {
            add_move(list, &move);
            move.kind = MOVE_PROMOTE;
            add_move(list, &move);
        }
        return;
    }
    add_move(list, &move);
}

static void add_step_by_delta(const Board* board, Teban side, int from, int dr, int dc, MoveList* list)
{
    int row = tsume_square_row(from);
    int col = tsume_square_col(from);
    int toRow = row + dr;
    int toCol = col + dc;
    if (in_board(toRow, toCol))
        add_step_move(board, side, from, tsume_square_index(toRow, toCol), list);
}

static void add_slide_by_delta(const Board* board, Teban side, int from, int dr, int dc, MoveList* list)
{
    int row = tsume_square_row(from) + dr;
    int col = tsume_square_col(from) + dc;
    while (in_board(row, col)) {
        int to = tsume_square_index(row, col);
        if (board->squares[to] != NO_KOMA) {
            if (tsume_board_square_side(board, to) != side)
                add_step_move(board, side, from, to, list);
            return;
        }
        add_step_move(board, side, from, to, list);
        row += dr;
        col += dc;
    }
}

static void generate_piece_moves(const Board* board, Teban side, int from, MoveList* list)
{
    Koma piece = board->squares[from];
    int forward = forward_direction(side);

    switch (piece) {
    case FU:
        add_step_by_delta(board, side, from, forward, 0, list);
        break;
    case KYO:
        add_slide_by_delta(board, side, from, forward, 0, list);
        break;
    case KEI:
        add_step_by_delta(board, side, from, forward * 2, -1, list);
        add_step_by_delta(board, side, from, forward * 2, 1, list);
        break;
    case GIN:
        add_step_by_delta(board, side, from, forward, -1, list);
        add_step_by_delta(board, side, from, forward, 0, list);
        add_step_by_delta(board, side, from, forward, 1, list);
        add_step_by_delta(board, side, from, -forward, -1, list);
        add_step_by_delta(board, side, from, -forward, 1, list);
        break;
    case KIN:
    case TO:
    case NARIKYO:
    case NARIKEI:
    case NARIGIN:
        add_step_by_delta(board, side, from, forward, -1, list);
        add_step_by_delta(board, side, from, forward, 0, list);
        add_step_by_delta(board, side, from, forward, 1, list);
        add_step_by_delta(board, side, from, 0, -1, list);
        add_step_by_delta(board, side, from, 0, 1, list);
        add_step_by_delta(board, side, from, -forward, 0, list);
        break;
    case KAKU:
        add_slide_by_delta(board, side, from, -1, -1, list);
        add_slide_by_delta(board, side, from, -1, 1, list);
        add_slide_by_delta(board, side, from, 1, -1, list);
        add_slide_by_delta(board, side, from, 1, 1, list);
        break;
    case UMA:
        add_slide_by_delta(board, side, from, -1, -1, list);
        add_slide_by_delta(board, side, from, -1, 1, list);
        add_slide_by_delta(board, side, from, 1, -1, list);
        add_slide_by_delta(board, side, from, 1, 1, list);
        add_step_by_delta(board, side, from, -1, 0, list);
        add_step_by_delta(board, side, from, 1, 0, list);
        add_step_by_delta(board, side, from, 0, -1, list);
        add_step_by_delta(board, side, from, 0, 1, list);
        break;
    case HISHA:
        add_slide_by_delta(board, side, from, -1, 0, list);
        add_slide_by_delta(board, side, from, 1, 0, list);
        add_slide_by_delta(board, side, from, 0, -1, list);
        add_slide_by_delta(board, side, from, 0, 1, list);
        break;
    case RYU:
        add_slide_by_delta(board, side, from, -1, 0, list);
        add_slide_by_delta(board, side, from, 1, 0, list);
        add_slide_by_delta(board, side, from, 0, -1, list);
        add_slide_by_delta(board, side, from, 0, 1, list);
        add_step_by_delta(board, side, from, -1, -1, list);
        add_step_by_delta(board, side, from, -1, 1, list);
        add_step_by_delta(board, side, from, 1, -1, list);
        add_step_by_delta(board, side, from, 1, 1, list);
        break;
    case GYOKU:
    case OU:
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr != 0 || dc != 0)
                    add_step_by_delta(board, side, from, dr, dc, list);
            }
        }
        break;
    case NO_KOMA:
        break;
    }
}

static bool dropped_piece_attacks_square(const Board* board, Koma piece, Teban side, int from, int target)
{
    if (target < 0)
        return false;

    int forward = forward_direction(side);
    int fromRow = tsume_square_row(from);
    int fromCol = tsume_square_col(from);
    int targetRow = tsume_square_row(target);
    int targetCol = tsume_square_col(target);
    int dr = targetRow - fromRow;
    int dc = targetCol - fromCol;

    switch (piece) {
    case FU:
        return dr == forward && dc == 0;
    case KYO:
        return dc == 0 && dr * forward > 0 && path_is_clear(board, fromRow, fromCol, targetRow, targetCol, forward, 0);
    case KEI:
        return dr == forward * 2 && (dc == -1 || dc == 1);
    case GIN:
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == -forward && (dc == -1 || dc == 1));
    case KIN:
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == 0 && (dc == -1 || dc == 1)) || (dr == -forward && dc == 0);
    case KAKU:
        return abs(dr) == abs(dc) && dr != 0 && path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, dc > 0 ? 1 : -1);
    case HISHA:
        if (dr == 0 && dc != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, 0, dc > 0 ? 1 : -1);
        if (dc == 0 && dr != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, 0);
        return false;
    case GYOKU:
    case OU:
    case TO:
    case NARIKYO:
    case NARIKEI:
    case NARIGIN:
    case UMA:
    case RYU:
    case NO_KOMA:
        return false;
    }
    return false;
}

static void generate_drop_moves(const Board* board, Teban side, bool onlyCheckingDrops, int targetSquare, MoveList* list)
{
    for (Koma piece = FU; piece < MOCHIGOMA_LIMIT; piece++) {
        if (board->mochigoma[side][piece] == 0)
            continue;
        for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
            if (board->squares[square] != NO_KOMA)
                continue;
            int row = tsume_square_row(square);
            int col = tsume_square_col(square);
            if (!can_drop_on_row(side, piece, row))
                continue;
            switch (piece) {
            case FU:
                if (file_has_unpromoted_pawn(board, side, col))
                    continue;
                break;
            case KYO:
            case KEI:
            case GIN:
            case KIN:
            case KAKU:
            case HISHA:
                break;
            case GYOKU:
            case OU:
            case TO:
            case NARIKYO:
            case NARIKEI:
            case NARIGIN:
            case UMA:
            case RYU:
            case NO_KOMA:
                continue;
            }
            if (onlyCheckingDrops && !dropped_piece_attacks_square(board, piece, side, square, targetSquare))
                continue;
            Move move = {
                .piece = piece,
                .captured = NO_KOMA,
                .side = side,
                .from = -1,
                .to = square,
                .kind = MOVE_DROP,
            };
            add_move(list, &move);
        }
    }
}

static void put_piece_on_board(Board* board, int square, Koma koma, Teban side)
{
    board->squares[square] = koma;
    BoardBitset* goteSquares = &board->goteSquares;
    uint64_t mask = 1ULL << (square % BITS_PER_WORD);
    if (koma == NO_KOMA) {
        goteSquares->words[square / BITS_PER_WORD] &= ~mask;
        return;
    }

    switch (side) {
    case SENTE:
        goteSquares->words[square / BITS_PER_WORD] &= ~mask;
        break;
    case GOTE:
        goteSquares->words[square / BITS_PER_WORD] |= mask;
        break;
    case TEBAN_COUNT:
        goteSquares->words[square / BITS_PER_WORD] &= ~mask;
        break;
    }
}

Board tsume_board_after_move(const Board* board, const Move* move)
{
    Board next = *board;
    Koma placedPiece = move->piece;
    switch (move->kind) {
    case MOVE_NORMAL:
        if (move->captured != NO_KOMA)
            next.mochigoma[move->side][tsume_unpromote(move->captured)]++;
        put_piece_on_board(&next, move->from, NO_KOMA, SENTE);
        break;
    case MOVE_PROMOTE:
        placedPiece = tsume_promote(move->piece);
        if (move->captured != NO_KOMA)
            next.mochigoma[move->side][tsume_unpromote(move->captured)]++;
        put_piece_on_board(&next, move->from, NO_KOMA, SENTE);
        break;
    case MOVE_DROP:
        next.mochigoma[move->side][move->piece]--;
        break;
    }
    put_piece_on_board(&next, move->to, placedPiece, move->side);
    return next;
}

static int find_king(const Board* board, Teban side)
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

static bool piece_attacks_square(const Board* board, int from, int target)
{
    Koma piece = board->squares[from];
    Teban side = tsume_board_square_side(board, from);
    int forward = forward_direction(side);
    int fromRow = tsume_square_row(from);
    int fromCol = tsume_square_col(from);
    int targetRow = tsume_square_row(target);
    int targetCol = tsume_square_col(target);
    int dr = targetRow - fromRow;
    int dc = targetCol - fromCol;

    switch (piece) {
    case FU:
        return dr == forward && dc == 0;
    case KYO:
        return dc == 0 && dr * forward > 0 && path_is_clear(board, fromRow, fromCol, targetRow, targetCol, forward, 0);
    case KEI:
        return dr == forward * 2 && (dc == -1 || dc == 1);
    case GIN:
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == -forward && (dc == -1 || dc == 1));
    case KIN:
    case TO:
    case NARIKYO:
    case NARIKEI:
    case NARIGIN:
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == 0 && (dc == -1 || dc == 1)) || (dr == -forward && dc == 0);
    case KAKU:
        return abs(dr) == abs(dc) && dr != 0 && path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, dc > 0 ? 1 : -1);
    case UMA:
        if (abs(dr) == abs(dc) && dr != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, dc > 0 ? 1 : -1);
        return (abs(dr) == 1 && dc == 0) || (dr == 0 && abs(dc) == 1);
    case HISHA:
        if (dr == 0 && dc != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, 0, dc > 0 ? 1 : -1);
        if (dc == 0 && dr != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, 0);
        return false;
    case RYU:
        if (dr == 0 && dc != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, 0, dc > 0 ? 1 : -1);
        if (dc == 0 && dr != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, 0);
        return abs(dr) == 1 && abs(dc) == 1;
    case GYOKU:
    case OU:
        return abs(dr) <= 1 && abs(dc) <= 1 && (dr != 0 || dc != 0);
    case NO_KOMA:
        return false;
    }

    return false;
}
bool tsume_is_in_check(const Board* board, Teban side)
{
    int kingSquare = find_king(board, side);
    if (kingSquare < 0)
        return true;
    Teban attacker = tsume_opponent(side);
    for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
        if (board->squares[square] == NO_KOMA || tsume_board_square_side(board, square) != attacker)
            continue;
        if (piece_attacks_square(board, square, kingSquare))
            return true;
    }
    return false;
}

void tsume_generate_legal_moves_with_scratch(const Board* board, Teban side, bool onlyCheckingMoves, MoveList* moves, MoveList* pseudoMoves)
{
    if (!pseudoMoves) {
        moves->count = 0;
        return;
    }
    pseudoMoves->count = 0;

    for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
        if (board->squares[square] != NO_KOMA && tsume_board_square_side(board, square) == side)
            generate_piece_moves(board, side, square, pseudoMoves);
    }
    int targetKing = onlyCheckingMoves ? find_king(board, tsume_opponent(side)) : -1;
    generate_drop_moves(board, side, onlyCheckingMoves, targetKing, pseudoMoves);

    moves->count = 0;
    for (int i = 0; i < pseudoMoves->count; i++) {
        Move move = pseudoMoves->moves[i];
        Board next = tsume_board_after_move(board, &move);
        if (tsume_is_in_check(&next, side))
            continue;
        if (onlyCheckingMoves && !tsume_is_in_check(&next, tsume_opponent(side)))
            continue;
        add_move(moves, &move);
    }
}

void tsume_generate_legal_moves(const Board* board, Teban side, bool onlyCheckingMoves, MoveList* moves)
{
    MoveList pseudoMoves = { 0 };
    tsume_generate_legal_moves_with_scratch(board, side, onlyCheckingMoves, moves, &pseudoMoves);
}
