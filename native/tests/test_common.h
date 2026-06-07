#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "tsume_shogi.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "assertion failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

#define ASSERT_EQ_INT(expected, actual) do { \
    int expectedValue = (expected); \
    int actualValue = (actual); \
    if (expectedValue != actualValue) { \
        fprintf(stderr, "assertion failed at %s:%d: expected %d, got %d\n", __FILE__, __LINE__, expectedValue, actualValue); \
        exit(1); \
    } \
} while (0)

static const char* SAMPLE_INPUT =
    "作品：人工サンプル\n"
    "後手の持駒：なし\n"
    "  ９ ８ ７ ６ ５ ４ ３ ２ １\n"
    "+---------------------------+\n"
    "| ・ ・ ・ ・v玉 ・ ・ ・ ・|一\n"
    "| ・ ・ ・ ・ 飛 ・ ・ ・ ・|二\n"
    "| ・ ・ ・ ・ ・ ・ ・ ・ ・|三\n"
    "| ・ ・ ・ ・ ・ ・ ・ ・ ・|四\n"
    "| ・ ・ ・ ・ ・ ・ ・ ・ ・|五\n"
    "| ・ ・ ・ ・ ・ ・ ・ ・ ・|六\n"
    "| ・ ・ ・ ・ ・ ・ ・ ・ ・|七\n"
    "| ・ ・ ・ ・ ・ ・ ・ ・ ・|八\n"
    "| ・ ・ ・ ・ 玉 ・ ・ ・ ・|九\n"
    "+---------------------------+\n"
    "先手の持駒：金\n";

#endif
