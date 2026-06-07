#include "test_common.h"

int main(void)
{
    tsume_init();
    Board board;
    TsumeParseResult result = tsume_parse_board_text(SAMPLE_INPUT, &board);
    ASSERT_EQ_INT(TSUME_OK, result.status);

    ASSERT_EQ_INT(NO_KOMA, board.squares[tsume_square_index(0, 0)]);
    ASSERT_EQ_INT(GYOKU, board.squares[tsume_square_index(0, 4)]);
    ASSERT_TRUE(tsume_board_is_gote_square(&board, tsume_square_index(0, 4)));
    ASSERT_EQ_INT(HISHA, board.squares[tsume_square_index(1, 4)]);
    ASSERT_TRUE(!tsume_board_is_gote_square(&board, tsume_square_index(1, 4)));
    ASSERT_EQ_INT(GYOKU, board.squares[tsume_square_index(8, 4)]);
    ASSERT_TRUE(!tsume_board_is_gote_square(&board, tsume_square_index(8, 4)));
    ASSERT_EQ_INT(1, board.mochigoma[SENTE][KIN]);
    ASSERT_EQ_INT(0, board.mochigoma[GOTE][FU]);

    printf("parser_test passed\n");
    return 0;
}
