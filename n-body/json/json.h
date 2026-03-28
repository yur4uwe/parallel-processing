#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "../arena/arena.h"

typedef enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOL,
    JSON_NULL
} json_type;

typedef struct json_value {
    json_type type;
    union {
        double number;
        uint8_t* string;
        bool boolean;
        struct {
            struct json_value** elements;
            int count;
        } array;
        struct {
            uint8_t** keys;
            struct json_value** values;
            int count;
        } object;
    } u;
} json_value;

json_value* parse_json(Arena* a, uint8_t* str, uint64_t str_len);

/**
 * JSON Assertion Macros
 *
 * Pattern requirements:
 * 1. An integer error code variable: `int ec = EXIT_SUCCESS;`
 * 2. A failure label: `assert_failed:`
 *
 * When an assertion fails, `ec` is set to `EXIT_FAILURE` and a `goto assert_failed;` is executed.
 */

#define JSON_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("[FAIL] %s: %s\n", __func__, (msg)); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)

#define JSON_ASSERT_NAME(exp, act, msg) \
    do { \
        if (strcmp((char*)(exp), (act)) != 0) { \
            printf("[FAIL] %s: %s, expected %s, got %s", __func__, (msg), (exp), (act)); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)

#define JSON_ASSERT_TYPE(v, t, msg) \
    do { \
        if (!(v)) { \
            printf("[FAIL] %s: %s (value is NULL, expected type " #t ")\n", __func__, (msg)); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } else if ((v)->type != (t)) { \
            printf("[FAIL] %s: %s (expected type " #t ", got %d)\n", __func__, (msg), (int)(v)->type); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)

#define JSON_ASSERT_NUMBER(v, n, msg) \
    do { \
        JSON_ASSERT_TYPE(v, JSON_NUMBER, msg); \
        if (fabs((v)->u.number - (n)) > 1e-6) { \
            printf("[FAIL] %s: %s (expected %f, got %f)\n", __func__, (msg), (double)(n), (v)->u.number); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)

#define JSON_ASSERT_BOOL(v, b, msg) \
    do { \
        JSON_ASSERT_TYPE(v, JSON_BOOL, msg); \
        if ((v)->u.boolean != (b)) { \
            printf("[FAIL] %s: %s (expected %s, got %s)\n", __func__, (msg), (b) ? "true" : "false", (v)->u.boolean ? "true" : "false"); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)

#define JSON_ASSERT_STRING(v, s, msg) \
    do { \
        JSON_ASSERT_TYPE(v, JSON_STRING, msg); \
        if (strcmp((char*)(v)->u.string, (s)) != 0) { \
            printf("[FAIL] %s: %s (expected \"%s\", got \"%s\")\n", __func__, (msg), (s), (char*)(v)->u.string); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)

#define JSON_ASSERT_NULL(v, msg) \
    JSON_ASSERT_TYPE(v, JSON_NULL, msg)

#define JSON_ASSERT_OBJECT(v, c, msg) \
    do { \
        JSON_ASSERT_TYPE(v, JSON_OBJECT, msg); \
        if ((v)->u.object.count != (c)) { \
            printf("[FAIL] %s: %s (expected %d keys, got %d)\n", __func__, (msg), (int)(c), (v)->u.object.count); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)

#define JSON_ASSERT_ARRAY(v, c, msg) \
    do { \
        JSON_ASSERT_TYPE(v, JSON_ARRAY, msg); \
        if ((v)->u.array.count != (c)) { \
            printf("[FAIL] %s: %s (expected %d elements, got %d)\n", __func__, (msg), (int)(c), (v)->u.array.count); \
            ec = EXIT_FAILURE; \
            goto assert_failed; \
        } \
    } while(0)
