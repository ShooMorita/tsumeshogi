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

    Move move = { board->squares[from], target, side, from, to, false, false };
    int fromRow = tsume_square_row(from);
    int toRow = tsume_square_row(to);
    if (tsume_can_promote(move.piece) && (is_promotion_zone(side, fromRow) || is_promotion_zone(side, toRow))) {
        if (must_promote(side, move.piece, toRow)) {
            move.promote = true;
            add_move(list, &move);
        } else {
            add_move(list, &move);
            move.promote = true;
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

static void generate_drop_moves(const Board* board, Teban side, MoveList* list)
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
            Move move = { piece, NO_KOMA, side, -1, square, true, false };
            add_move(list, &move);
        }
    }
}

void tsume_apply_move(Board* board, const Move* move)
{
    Koma placedPiece = move->promote ? tsume_promote(move->piece) : move->piece;
    if (move->drop) {
        board->mochigoma[move->side][move->piece]--;
    } else {
        if (move->captured != NO_KOMA)
            board->mochigoma[move->side][tsume_unpromote(move->captured)]++;
        tsume_board_clear_square(board, move->from);
    }
    tsume_board_set_piece(board, move->to, placedPiece, move->side);
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
    MoveList* pseudoMoves = (MoveList*)calloc(1, sizeof(*pseudoMoves));
    if (!pseudoMoves)
        return false;

    Teban side = tsume_board_square_side(board, from);
    generate_piece_moves(board, side, from, pseudoMoves);
    for (int i = 0; i < pseudoMoves->count; i++) {
        if (pseudoMoves->moves[i].to == target) {
            free(pseudoMoves);
            return true;
        }
    }
    free(pseudoMoves);
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

void tsume_generate_legal_moves(const Board* board, Teban side, bool onlyCheckingMoves, MoveList* moves)
{
    MoveList* pseudoMoves = (MoveList*)calloc(1, sizeof(*pseudoMoves));
    if (!pseudoMoves) {
        moves->count = 0;
        return;
    }

    for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
        if (board->squares[square] != NO_KOMA && tsume_board_square_side(board, square) == side)
            generate_piece_moves(board, side, square, pseudoMoves);
    }
    generate_drop_moves(board, side, pseudoMoves);

    moves->count = 0;
    for (int i = 0; i < pseudoMoves->count; i++) {
        Move move = pseudoMoves->moves[i];
        Board next = *board;
        tsume_apply_move(&next, &move);
        if (tsume_is_in_check(&next, side))
            continue;
        if (onlyCheckingMoves && !tsume_is_in_check(&next, tsume_opponent(side)))
            continue;
        add_move(moves, &move);
    }
    free(pseudoMoves);
}
