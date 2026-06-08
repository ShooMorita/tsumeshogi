#include "test_common.h"

static bool has_move_to(const MoveList* list, int to)
{
    for (int i = 0; i < list->count; i++) {
        if (list->moves[i].to == to)
            return true;
    }
    return false;
}

int main(void)
{
    Board board;
    tsume_board_init(&board);
    tsume_board_set_piece(&board, tsume_square_index(8, 4), GYOKU, SENTE);
    tsume_board_set_piece(&board, tsume_square_index(0, 4), GYOKU, GOTE);
    tsume_board_set_piece(&board, tsume_square_index(6, 4), FU, SENTE);

    MoveList moves = { 0 };
    tsume_generate_legal_moves(&board, SENTE, false, &moves);
    ASSERT_TRUE(has_move_to(&moves, tsume_square_index(5, 4)));

    Move pawnMove = { 0 };
    for (int i = 0; i < moves.count; i++) {
        if (moves.moves[i].to == tsume_square_index(5, 4)) {
            pawnMove = moves.moves[i];
            break;
        }
    }
    Board nextBoard = tsume_board_after_move(&board, &pawnMove);
    ASSERT_EQ_INT(FU, nextBoard.squares[tsume_square_index(5, 4)]);
    ASSERT_EQ_INT(NO_KOMA, nextBoard.squares[tsume_square_index(6, 4)]);
    ASSERT_EQ_INT(FU, board.squares[tsume_square_index(6, 4)]);
    ASSERT_EQ_INT(NO_KOMA, board.squares[tsume_square_index(5, 4)]);

    board.mochigoma[SENTE][KIN] = 1;
    moves.count = 0;
    tsume_generate_legal_moves(&board, SENTE, false, &moves);
    ASSERT_TRUE(has_move_to(&moves, tsume_square_index(4, 4)));

    tsume_board_set_piece(&board, tsume_square_index(1, 4), HISHA, SENTE);
    ASSERT_TRUE(tsume_is_in_check(&board, GOTE));

    printf("movegen_test passed\n");
    return 0;
}
