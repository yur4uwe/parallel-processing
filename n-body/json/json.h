#pragma once
#include <stdbool.h>
#include <stdint.h>

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
            char** keys;
            struct json_value** values;
            int count;
        } object;
    } u;
} json_value;

json_value* parse_json(uint8_t* str);

