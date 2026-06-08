#include "tsume_shogi.h"

#include <string.h>

static const char* KOMA_NAMES[KOMA_KIND_COUNT] = {
    "歩", "香", "桂", "銀", "金", "角", "飛", "玉",
    "王", "と", "杏", "圭", "全", "馬", "龍"
};

static const char* KOMA_CODES[KOMA_KIND_COUNT] = {
    "FU", "KYO", "KEI", "GIN", "KIN", "KAKU", "HISHA", "GYOKU",
    "OU", "TO", "NARIKYO", "NARIKEI", "NARIGIN", "UMA", "RYU"
};

void tsume_init(void)
{
}

const char* tsume_koma_name(Koma koma)
{
    return (koma >= 0 && koma < KOMA_KIND_COUNT) ? KOMA_NAMES[koma] : "";
}

const char* tsume_koma_code(Koma koma)
{
    return (koma >= 0 && koma < KOMA_KIND_COUNT) ? KOMA_CODES[koma] : "NO_KOMA";
}

const char* tsume_teban_name(Teban side)
{
    return side == GOTE ? "gote" : "sente";
}

Teban tsume_opponent(Teban side)
{
    return side == SENTE ? GOTE : SENTE;
}

int tsume_square_index(int row, int col)
{
    return row * BOARD_SIZE + col;
}

int tsume_square_row(int square)
{
    return square / BOARD_SIZE;
}

int tsume_square_col(int square)
{
    return square % BOARD_SIZE;
}

static void set_gote_square(Board* board, int square)
{
    board->goteSquares.words[square / BITS_PER_WORD] |= 1ULL << (square % BITS_PER_WORD);
}

static void clear_gote_square(Board* board, int square)
{
    board->goteSquares.words[square / BITS_PER_WORD] &= ~(1ULL << (square % BITS_PER_WORD));
}

bool tsume_board_is_gote_square(const Board* board, int square)
{
    return (board->goteSquares.words[square / BITS_PER_WORD] & (1ULL << (square % BITS_PER_WORD))) != 0;
}

Teban tsume_board_square_side(const Board* board, int square)
{
    return tsume_board_is_gote_square(board, square) ? GOTE : SENTE;
}

static void init_board(Board* board)
{
    memset(board, 0, sizeof(*board));
    for (int square = 0; square < BOARD_SQUARE_COUNT; square++)
        board->squares[square] = NO_KOMA;
}

static void put_piece(Board* board, int square, Koma koma, Teban side)
{
    board->squares[square] = koma;
    if (koma == NO_KOMA)
        clear_gote_square(board, square);
    else if (side == GOTE)
        set_gote_square(board, square);
    else
        clear_gote_square(board, square);
}

Board tsume_empty_board(void)
{
    Board board;
    init_board(&board);
    return board;
}

Board tsume_board_with_piece(const Board* board, int square, Koma koma, Teban side)
{
    Board next = *board;
    put_piece(&next, square, koma, side);
    return next;
}

Board tsume_board_without_piece(const Board* board, int square)
{
    Board next = *board;
    put_piece(&next, square, NO_KOMA, SENTE);
    return next;
}

void tsume_board_init(Board* board)
{
    init_board(board);
}

void tsume_board_set_piece(Board* board, int square, Koma koma, Teban side)
{
    put_piece(board, square, koma, side);
}

void tsume_board_clear_square(Board* board, int square)
{
    put_piece(board, square, NO_KOMA, SENTE);
}

Koma tsume_promote(Koma koma)
{
    switch (koma) {
    case FU:
        return TO;
    case KYO:
        return NARIKYO;
    case KEI:
        return NARIKEI;
    case GIN:
        return NARIGIN;
    case KAKU:
        return UMA;
    case HISHA:
        return RYU;
    default:
        return koma;
    }
}

Koma tsume_unpromote(Koma koma)
{
    switch (koma) {
    case TO:
        return FU;
    case NARIKYO:
        return KYO;
    case NARIKEI:
        return KEI;
    case NARIGIN:
        return GIN;
    case UMA:
        return KAKU;
    case RYU:
        return HISHA;
    default:
        return koma;
    }
}

bool tsume_can_promote(Koma koma)
{
    return koma == FU || koma == KYO || koma == KEI || koma == GIN || koma == KAKU || koma == HISHA;
}

static uint64_t hash_mix(uint64_t hash, uint64_t value)
{
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    return hash;
}

uint64_t tsume_position_hash(const Board* board, Teban side, int remainingPly)
{
    uint64_t hash = 1469598103934665603ULL;
    for (int square = 0; square < BOARD_SQUARE_COUNT; square++) {
        if (board->squares[square] == NO_KOMA)
            continue;
        uint64_t value = (uint64_t)(square + 1) * 131ULL;
        value ^= (uint64_t)(board->squares[square] + 1) * 911ULL;
        value ^= (uint64_t)(tsume_board_square_side(board, square) + 1) * 3571ULL;
        hash = hash_mix(hash, value);
    }
    for (int owner = 0; owner < TEBAN_COUNT; owner++) {
        for (int koma = 0; koma < MOCHIGOMA_LIMIT; koma++) {
            uint8_t count = board->mochigoma[owner][koma];
            if (count)
                hash = hash_mix(hash, (uint64_t)(owner + 1) * 1000003ULL + (uint64_t)(koma + 1) * 97ULL + count);
        }
    }
    hash = hash_mix(hash, (uint64_t)(side + 1));
    hash = hash_mix(hash, (uint64_t)(remainingPly + 1));
    return hash;
}
