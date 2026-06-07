#include "test_common.h"

static void build_mate_in_one(Board* board)
{
    tsume_board_init(board);
    tsume_board_set_piece(board, tsume_square_index(8, 8), GYOKU, SENTE);
    tsume_board_set_piece(board, tsume_square_index(0, 4), GYOKU, GOTE);
    tsume_board_set_piece(board, tsume_square_index(1, 4), HISHA, SENTE);
    tsume_board_set_piece(board, tsume_square_index(0, 3), KIN, SENTE);
    tsume_board_set_piece(board, tsume_square_index(0, 5), KIN, SENTE);
    tsume_board_set_piece(board, tsume_square_index(1, 3), KIN, SENTE);
    tsume_board_set_piece(board, tsume_square_index(1, 5), KIN, SENTE);
    tsume_board_set_piece(board, tsume_square_index(1, 2), KAKU, SENTE);
    tsume_board_set_piece(board, tsume_square_index(1, 6), KAKU, SENTE);
}

static void build_drop_mate(Board* board)
{
    tsume_board_init(board);
    tsume_board_set_piece(board, tsume_square_index(8, 8), GYOKU, SENTE);
    tsume_board_set_piece(board, tsume_square_index(0, 4), GYOKU, GOTE);
    tsume_board_set_piece(board, tsume_square_index(0, 3), FU, GOTE);
    tsume_board_set_piece(board, tsume_square_index(0, 5), FU, GOTE);
    tsume_board_set_piece(board, tsume_square_index(1, 3), FU, GOTE);
    tsume_board_set_piece(board, tsume_square_index(1, 5), FU, GOTE);
    tsume_board_set_piece(board, tsume_square_index(3, 2), KAKU, SENTE);
    board->mochigoma[SENTE][HISHA] = 1;
}

static bool line_contains_drop(const TsumeLine* line)
{
    for (int i = 0; i < line->count; i++) {
        if (line->moves[i].drop)
            return true;
    }
    return false;
}

int main(void)
{
    Board board;
    TsumeLine line = { 0 };

    build_mate_in_one(&board);
    TsumeSolveResult result = tsume_solve_dfpn(&board, 1, &line);
    ASSERT_EQ_INT(TSUME_OK, result.status);
    ASSERT_TRUE(line.count >= 1);

    build_drop_mate(&board);
    line.count = 0;
    result = tsume_solve_dfpn(&board, 1, &line);
    ASSERT_EQ_INT(TSUME_OK, result.status);
    ASSERT_TRUE(line.count >= 1);
    ASSERT_TRUE(line_contains_drop(&line));

    result = tsume_solve_dfpn(&board, 2, &line);
    ASSERT_EQ_INT(TSUME_INVALID_DEPTH, result.status);

    tsume_board_init(&board);
    tsume_board_set_piece(&board, tsume_square_index(8, 8), GYOKU, SENTE);
    tsume_board_set_piece(&board, tsume_square_index(0, 4), GYOKU, GOTE);
    result = tsume_solve_dfpn(&board, 1, &line);
    ASSERT_EQ_INT(TSUME_NO_MATE, result.status);

    printf("solver_test passed\n");
    return 0;
}
