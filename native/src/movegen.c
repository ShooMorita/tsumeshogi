#include "tsume_shogi.h"

#include <stdlib.h>
#include <string.h>

static bool in_board(int row, int col)
{
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

static bool is_promoted_gold_like(Koma koma)
{
    return koma == TO || koma == NARIKYO || koma == NARIKEI || koma == NARIGIN;
}

static bool is_promotion_zone(Teban side, int row)
{
    return side == SENTE ? row <= 2 : row >= 6;
}

static bool must_promote(Teban side, Koma koma, int toRow)
{
    if (side == SENTE)
        return (koma == FU || koma == KYO) ? toRow == 0 : (koma == KEI && toRow <= 1);
    return (koma == FU || koma == KYO) ? toRow == 8 : (koma == KEI && toRow >= 7);
}

static bool can_drop_on_row(Teban side, Koma koma, int row)
{
    if (side == SENTE) {
        if ((koma == FU || koma == KYO) && row == 0)
            return false;
        if (koma == KEI && row <= 1)
            return false;
    } else {
        if ((koma == FU || koma == KYO) && row == 8)
            return false;
        if (koma == KEI && row >= 7)
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
    int forward = side == SENTE ? -1 : 1;

    if (piece == FU) {
        add_step_by_delta(board, side, from, forward, 0, list);
    } else if (piece == KYO) {
        add_slide_by_delta(board, side, from, forward, 0, list);
    } else if (piece == KEI) {
        add_step_by_delta(board, side, from, forward * 2, -1, list);
        add_step_by_delta(board, side, from, forward * 2, 1, list);
    } else if (piece == GIN) {
        add_step_by_delta(board, side, from, forward, -1, list);
        add_step_by_delta(board, side, from, forward, 0, list);
        add_step_by_delta(board, side, from, forward, 1, list);
        add_step_by_delta(board, side, from, -forward, -1, list);
        add_step_by_delta(board, side, from, -forward, 1, list);
    } else if (piece == KIN || is_promoted_gold_like(piece)) {
        add_step_by_delta(board, side, from, forward, -1, list);
        add_step_by_delta(board, side, from, forward, 0, list);
        add_step_by_delta(board, side, from, forward, 1, list);
        add_step_by_delta(board, side, from, 0, -1, list);
        add_step_by_delta(board, side, from, 0, 1, list);
        add_step_by_delta(board, side, from, -forward, 0, list);
    } else if (piece == KAKU || piece == UMA) {
        add_slide_by_delta(board, side, from, -1, -1, list);
        add_slide_by_delta(board, side, from, -1, 1, list);
        add_slide_by_delta(board, side, from, 1, -1, list);
        add_slide_by_delta(board, side, from, 1, 1, list);
        if (piece == UMA) {
            add_step_by_delta(board, side, from, -1, 0, list);
            add_step_by_delta(board, side, from, 1, 0, list);
            add_step_by_delta(board, side, from, 0, -1, list);
            add_step_by_delta(board, side, from, 0, 1, list);
        }
    } else if (piece == HISHA || piece == RYU) {
        add_slide_by_delta(board, side, from, -1, 0, list);
        add_slide_by_delta(board, side, from, 1, 0, list);
        add_slide_by_delta(board, side, from, 0, -1, list);
        add_slide_by_delta(board, side, from, 0, 1, list);
        if (piece == RYU) {
            add_step_by_delta(board, side, from, -1, -1, list);
            add_step_by_delta(board, side, from, -1, 1, list);
            add_step_by_delta(board, side, from, 1, -1, list);
            add_step_by_delta(board, side, from, 1, 1, list);
        }
    } else if (piece == GYOKU || piece == OU) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr != 0 || dc != 0)
                    add_step_by_delta(board, side, from, dr, dc, list);
            }
        }
    }
}

static bool dropped_piece_attacks_square(const Board* board, Koma piece, Teban side, int from, int target)
{
    if (target < 0)
        return false;

    int forward = side == SENTE ? -1 : 1;
    int fromRow = tsume_square_row(from);
    int fromCol = tsume_square_col(from);
    int targetRow = tsume_square_row(target);
    int targetCol = tsume_square_col(target);
    int dr = targetRow - fromRow;
    int dc = targetCol - fromCol;

    if (piece == FU)
        return dr == forward && dc == 0;
    if (piece == KYO)
        return dc == 0 && dr * forward > 0 && path_is_clear(board, fromRow, fromCol, targetRow, targetCol, forward, 0);
    if (piece == KEI)
        return dr == forward * 2 && (dc == -1 || dc == 1);
    if (piece == GIN)
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == -forward && (dc == -1 || dc == 1));
    if (piece == KIN)
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == 0 && (dc == -1 || dc == 1)) || (dr == -forward && dc == 0);
    if (piece == KAKU)
        return abs(dr) == abs(dc) && dr != 0 && path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, dc > 0 ? 1 : -1);
    if (piece == HISHA) {
        if (dr == 0 && dc != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, 0, dc > 0 ? 1 : -1);
        if (dc == 0 && dr != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, 0);
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
            if (piece == FU && file_has_unpromoted_pawn(board, side, col))
                continue;
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
    if (koma != NO_KOMA && side == GOTE)
        goteSquares->words[square / BITS_PER_WORD] |= mask;
    else
        goteSquares->words[square / BITS_PER_WORD] &= ~mask;
}

Board tsume_board_after_move(const Board* board, const Move* move)
{
    Board next = *board;
    Koma placedPiece = move->kind == MOVE_PROMOTE ? tsume_promote(move->piece) : move->piece;
    if (move->kind == MOVE_DROP) {
        next.mochigoma[move->side][move->piece]--;
    } else {
        if (move->captured != NO_KOMA)
            next.mochigoma[move->side][tsume_unpromote(move->captured)]++;
        put_piece_on_board(&next, move->from, NO_KOMA, SENTE);
    }
    put_piece_on_board(&next, move->to, placedPiece, move->side);
    return next;
}

void tsume_apply_move(Board* board, const Move* move)
{
    Koma placedPiece = move->kind == MOVE_PROMOTE ? tsume_promote(move->piece) : move->piece;
    if (move->kind == MOVE_DROP) {
        board->mochigoma[move->side][move->piece]--;
    } else {
        if (move->captured != NO_KOMA)
            board->mochigoma[move->side][tsume_unpromote(move->captured)]++;
        put_piece_on_board(board, move->from, NO_KOMA, SENTE);
    }
    put_piece_on_board(board, move->to, placedPiece, move->side);
}

static int find_king(const Board* board, Teban side)
{
    for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
        Koma piece = board->squares[square];
        if ((piece == GYOKU || piece == OU) && tsume_board_square_side(board, square) == side)
            return square;
    }
    return -1;
}

static bool piece_attacks_square(const Board* board, int from, int target)
{
    Koma piece = board->squares[from];
    Teban side = tsume_board_square_side(board, from);
    int forward = side == SENTE ? -1 : 1;
    int fromRow = tsume_square_row(from);
    int fromCol = tsume_square_col(from);
    int targetRow = tsume_square_row(target);
    int targetCol = tsume_square_col(target);
    int dr = targetRow - fromRow;
    int dc = targetCol - fromCol;

    if (piece == FU)
        return dr == forward && dc == 0;
    if (piece == KYO)
        return dc == 0 && dr * forward > 0 && path_is_clear(board, fromRow, fromCol, targetRow, targetCol, forward, 0);
    if (piece == KEI)
        return dr == forward * 2 && (dc == -1 || dc == 1);
    if (piece == GIN)
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == -forward && (dc == -1 || dc == 1));
    if (piece == KIN || is_promoted_gold_like(piece))
        return (dr == forward && (dc == -1 || dc == 0 || dc == 1)) || (dr == 0 && (dc == -1 || dc == 1)) || (dr == -forward && dc == 0);
    if (piece == KAKU || piece == UMA) {
        if (abs(dr) == abs(dc) && dr != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, dc > 0 ? 1 : -1);
        if (piece == UMA)
            return (abs(dr) == 1 && dc == 0) || (dr == 0 && abs(dc) == 1);
        return false;
    }
    if (piece == HISHA || piece == RYU) {
        if (dr == 0 && dc != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, 0, dc > 0 ? 1 : -1);
        if (dc == 0 && dr != 0)
            return path_is_clear(board, fromRow, fromCol, targetRow, targetCol, dr > 0 ? 1 : -1, 0);
        if (piece == RYU)
            return abs(dr) == 1 && abs(dc) == 1;
        return false;
    }
    if (piece == GYOKU || piece == OU)
        return abs(dr) <= 1 && abs(dc) <= 1 && (dr != 0 || dc != 0);

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
