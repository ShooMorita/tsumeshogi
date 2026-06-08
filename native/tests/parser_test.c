#include "test_common.h"

static void test_board_text_parser(void)
{
    TsumeParseBoardResult result = tsume_parse_board_text_value(SAMPLE_INPUT);
    ASSERT_EQ_INT(TSUME_OK, result.status);

    Board board = result.board;
    ASSERT_EQ_INT(NO_KOMA, board.squares[tsume_square_index(0, 0)]);
    ASSERT_EQ_INT(GYOKU, board.squares[tsume_square_index(0, 4)]);
    ASSERT_TRUE(tsume_board_is_gote_square(&board, tsume_square_index(0, 4)));
    ASSERT_EQ_INT(HISHA, board.squares[tsume_square_index(1, 4)]);
    ASSERT_TRUE(!tsume_board_is_gote_square(&board, tsume_square_index(1, 4)));
    ASSERT_EQ_INT(GYOKU, board.squares[tsume_square_index(8, 4)]);
    ASSERT_TRUE(!tsume_board_is_gote_square(&board, tsume_square_index(8, 4)));
    ASSERT_EQ_INT(1, board.mochigoma[SENTE][KIN]);
    ASSERT_EQ_INT(0, board.mochigoma[GOTE][FU]);
}

static void test_kifu_replay_parser(void)
{
    const char* input =
        "場所：将棋テスト\n"
        "手合割：平手\n"
        "初期局面：通常\n"
        "手数----指手---------消費時間--\n"
        "   1 ７六歩(77)    ( 0:00/00:00:00)\n"
        "   2 ３四歩(33)    ( 0:00/00:00:00)\n"
        "   3 ２二角成(88)  ( 0:00/00:00:00)\n"
        "   4 同　銀(31)    ( 0:00/00:00:00)\n"
        "   5 ８八角打      ( 0:00/00:00:00)\n"
        "   6 投了          ( 0:00/00:00:00)\n"
        "まで5手で先手の勝ち\n";

    TsumeParseGameResult game = tsume_parse_game_text_value(input);
    ASSERT_EQ_INT(TSUME_OK, game.status);
    ASSERT_EQ_INT(5, game.moveCount);
    ASSERT_EQ_INT(KAKU, game.initialBoard.squares[tsume_square_index(7, 1)]);
    ASSERT_TRUE(!tsume_board_is_gote_square(&game.initialBoard, tsume_square_index(7, 1)));
    ASSERT_EQ_INT(MOVE_DROP, game.moves[4].kind);
    ASSERT_EQ_INT(tsume_square_index(7, 1), game.moves[4].to);

    TsumeParseBoardResult final = tsume_parse_board_text_value(input);
    ASSERT_EQ_INT(TSUME_OK, final.status);
    ASSERT_EQ_INT(KAKU, final.board.squares[tsume_square_index(7, 1)]);
    ASSERT_TRUE(!tsume_board_is_gote_square(&final.board, tsume_square_index(7, 1)));
    ASSERT_EQ_INT(GIN, final.board.squares[tsume_square_index(1, 7)]);
    ASSERT_TRUE(tsume_board_is_gote_square(&final.board, tsume_square_index(1, 7)));
    ASSERT_EQ_INT(0, final.board.mochigoma[SENTE][KAKU]);
    ASSERT_EQ_INT(1, final.board.mochigoma[GOTE][KAKU]);
}

static void test_same_pawn_and_drop(void)
{
    const char* input =
        "手合割：平手\n"
        "初期局面：通常\n"
        "手数----指手---------消費時間--\n"
        "   1 ２六歩(27)\n"
        "   2 ８四歩(83)\n"
        "   3 ２五歩(26)\n"
        "   4 ８五歩(84)\n"
        "   5 ２四歩(25)\n"
        "   6 同　歩(23)\n"
        "   7 ７八金(69)\n"
        "   8 ２七歩打\n";

    TsumeParseGameResult game = tsume_parse_game_text_value(input);
    ASSERT_EQ_INT(TSUME_OK, game.status);
    ASSERT_EQ_INT(8, game.moveCount);
    ASSERT_EQ_INT(MOVE_NORMAL, game.moves[5].kind);
    ASSERT_EQ_INT(tsume_square_index(3, 7), game.moves[5].to);
    ASSERT_EQ_INT(MOVE_DROP, game.moves[7].kind);

    TsumeParseBoardResult final = tsume_parse_board_text_value(input);
    ASSERT_EQ_INT(TSUME_OK, final.status);
    ASSERT_EQ_INT(FU, final.board.squares[tsume_square_index(6, 7)]);
    ASSERT_TRUE(tsume_board_is_gote_square(&final.board, tsume_square_index(6, 7)));
    ASSERT_EQ_INT(0, final.board.mochigoma[GOTE][FU]);
}

static void test_promoted_piece_name_move(void)
{
    const char* input =
        "手合割：平手\n"
        "初期局面：通常\n"
        "手数----指手---------消費時間--\n"
        "   1 ６八銀(79)\n"
        "   2 ８四歩(83)\n"
        "   3 ５八銀成(68)\n"
        "   4 ８五歩(84)\n"
        "   5 ４八成銀(58)\n";

    TsumeParseGameResult game = tsume_parse_game_text_value(input);
    ASSERT_EQ_INT(TSUME_OK, game.status);
    ASSERT_EQ_INT(5, game.moveCount);
    ASSERT_EQ_INT(NARIGIN, game.moves[4].piece);

    TsumeParseBoardResult final = tsume_parse_board_text_value(input);
    ASSERT_EQ_INT(TSUME_OK, final.status);
    ASSERT_EQ_INT(NARIGIN, final.board.squares[tsume_square_index(7, 5)]);
    ASSERT_TRUE(!tsume_board_is_gote_square(&final.board, tsume_square_index(7, 5)));
}

int main(void)
{
    test_board_text_parser();
    test_kifu_replay_parser();
    test_same_pawn_and_drop();
    test_promoted_piece_name_move();

    printf("parser_test passed\n");
    return 0;
}
