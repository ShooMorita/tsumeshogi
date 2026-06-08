#ifndef TSUME_SHOGI_H
#define TSUME_SHOGI_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_SIZE 9
#define BOARD_SQUARE_COUNT (BOARD_SIZE * BOARD_SIZE)
#define BITS_PER_WORD 64
#define MAX_MOVES 768
#define MAX_SOLUTION_MOVES 128
#define MAX_KIFU_MOVES 512
#define DFPN_TABLE_SIZE 65536

typedef struct {
    uint64_t words[2];
} BoardBitset;

typedef enum {
    SENTE,
    GOTE,

    TEBAN_COUNT,
} Teban;

typedef enum {
    FU,
    KYO,
    KEI,
    GIN,
    KIN,
    KAKU,
    HISHA,
    GYOKU,

    OU,
    TO,
    NARIKYO,
    NARIKEI,
    NARIGIN,
    UMA,
    RYU,

    KOMA_KIND_COUNT,
    NO_KOMA = KOMA_KIND_COUNT,
} Koma;

#define MOCHIGOMA_LIMIT GYOKU

typedef struct {
    Koma squares[BOARD_SQUARE_COUNT];
    BoardBitset goteSquares;
    uint8_t mochigoma[TEBAN_COUNT][MOCHIGOMA_LIMIT];
} Board;

typedef enum {
    TSUME_OK,
    TSUME_PARSE_ERROR,
    TSUME_INVALID_DEPTH,
    TSUME_NO_MATE,
    TSUME_TIMEOUT,
} TsumeStatus;

typedef struct {
    TsumeStatus status;
    char message[128];
    Board board;
} TsumeParseBoardResult;

typedef enum {
    MOVE_NORMAL,
    MOVE_PROMOTE,
    MOVE_DROP,
} MoveKind;

typedef struct {
    Koma piece;
    Koma captured;
    Teban side;
    int from;
    int to;
    MoveKind kind;
} Move;

typedef struct {
    TsumeStatus status;
    char message[128];
    Board initialBoard;
    Move moves[MAX_KIFU_MOVES];
    int moveCount;
} TsumeParseGameResult;

typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

typedef struct {
    Move moves[MAX_SOLUTION_MOVES];
    int count;
} TsumeLine;

typedef struct {
    TsumeStatus status;
    int maxPly;
    uint64_t nodes;
    char message[128];
} TsumeSolveResult;

const char* tsume_koma_name(Koma koma);
const char* tsume_koma_code(Koma koma);
const char* tsume_teban_name(Teban side);
Teban tsume_opponent(Teban side);

int tsume_square_index(int row, int col);
int tsume_square_row(int square);
int tsume_square_col(int square);
void tsume_board_init(Board* board);
void tsume_board_set_piece(Board* board, int square, Koma koma, Teban side);
bool tsume_board_is_gote_square(const Board* board, int square);
Teban tsume_board_square_side(const Board* board, int square);
uint64_t tsume_position_hash(const Board* board, Teban side, int remainingPly);

Koma tsume_promote(Koma koma);
Koma tsume_unpromote(Koma koma);
bool tsume_can_promote(Koma koma);

TsumeParseBoardResult tsume_parse_board_text_value(const char* input);
TsumeParseGameResult tsume_parse_game_text_value(const char* input);

bool tsume_is_in_check(const Board* board, Teban side);
void tsume_generate_legal_moves(const Board* board, Teban side, bool onlyCheckingMoves, MoveList* moves);
void tsume_generate_legal_moves_with_scratch(const Board* board, Teban side, bool onlyCheckingMoves, MoveList* moves, MoveList* pseudoMoves);
Board tsume_board_after_move(const Board* board, const Move* move);

TsumeSolveResult tsume_solve_dfpn(const Board* board, int maxPly, TsumeLine* line);

#endif
