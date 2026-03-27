#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../arena/arena.h"
#include "json.h"

void parse_array(Arena* a, json_value* v, uint8_t* str, size_t* pos);
void parse_object(Arena* a, json_value* v, uint8_t* str, size_t* pos);

void skip_whitespace(uint8_t* str, size_t* pos) {
    while (isspace(str[*pos])) {
        (*pos)++;
    }
}

int is_escapeable(uint8_t ch) {
    switch (ch) {
        case '\\':
        case 'n':
        case 'r':
        case 'b':
        case 't':
        case 'f':
        case '"':
            return EXIT_SUCCESS;
        default:
            return EXIT_FAILURE;
    };
}

uint8_t* parse_string(Arena* a, uint8_t* json_str, size_t* pos) {
    int strlen = 0;
    while (json_str[*pos] != '"') {
        if (json_str[*pos] == '\\') {
            // handle escapes
            (*pos)++;
            strlen++;
            if (is_escapeable(json_str[*pos]) == EXIT_FAILURE) {
                printf("illegal escaped character: %c", json_str[*pos]);
                return NULL;
            }
        }
        (*pos)++;
        strlen++;
    }

    uint8_t* val_str = arena_alloc(a, sizeof(uint8_t) * strlen);

    for (int i = strlen - 1; i >= 0; i--) {
        val_str[strlen - i - 1] = json_str[*pos - i];
    }

    (*pos)++;  // skip '"'
    return val_str;
}

json_value* parse_constant(Arena* a, uint8_t* json_str, size_t* pos,
                           uint8_t* expected_constant, int exp_const_len) {
    uint8_t constant[6];
    strncpy((char*)constant, (char*)(&json_str[*pos]), exp_const_len);
    if (strncmp((char*)constant, (char*)expected_constant, exp_const_len) == EXIT_SUCCESS &&
        (constant[exp_const_len + 1] == ' ' ||
         constant[exp_const_len + 1] == ',')) {
        json_value* val = arena_alloc(a, sizeof(json_value));
        (*pos) += exp_const_len;  // do not skip ',' for state management sake
        return val;
    }
    printf("[ERROR] expected '%s' constant, got %s", expected_constant,
           constant);
    return NULL;
}

void parse_object(Arena* a, json_value* v, uint8_t* str, size_t* pos) {
    (*pos)++;  // will skip '{'
    v->type = JSON_OBJECT;

    int kvp_count = 0;
    bool parsing_key = true;
    bool parsing_value = false;

    uint8_t** keys = arena_alloc(a, sizeof(char*) * 10);
    json_value** values = arena_alloc(a, sizeof(json_value*) * 10);
    uint8_t constant[6] = {0};
    json_value* buf_val = NULL;
    uint8_t* buf_string = NULL;

parse_enter:

    skip_whitespace(str, pos);
    switch (str[*pos]) {
        case '"':
            buf_string = parse_string(a, str, pos);
            if (buf_string == NULL) {
                v = NULL;
                return;
            }

            if (parsing_key) {
                keys[kvp_count] = buf_string;
                kvp_count++;
            } else if (parsing_value) {
                buf_val = arena_alloc(a, sizeof(json_value));
                buf_val->type = JSON_STRING;
                buf_val->u.string = buf_string;
            }

            buf_string = NULL;
            break;
        case ':':
            if (parsing_value) {
                printf("encountered ':' when looking for value");
                v = NULL;
                return;
            }
            (*pos)++;
            parsing_key = false;
            parsing_value = true;
            break;
        case 't':
            if (parsing_key) {
                printf(
                    "encountered 't' (possible 'true') when looking for key");
                v = NULL;
                return;
            }
            buf_val = parse_constant(a, str, pos, "true", 4);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }
            buf_val->type = JSON_BOOL;
            buf_val->u.boolean = true;
            break;
        case 'f':
            if (parsing_key) {
                printf(
                    "encountered 'f' (possible 'false') when looking for key");
                v = NULL;
                return;
            }
            buf_val = parse_constant(a, str, pos, "false", 5);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }
            buf_val->type = JSON_BOOL;
            buf_val->u.boolean = false;
            break;
        case 'n':
            if (parsing_key) {
                printf(
                    "encountered 'n' (possible 'null') when looking for key");
                v = NULL;
                return;
            }
            buf_val = parse_constant(a, str, pos, "null", 4);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }
            buf_val->type = JSON_NULL;
            break;
        case '{':
            if (parsing_key) {
                printf("encountered '{' when looking for key");
                v = NULL;
                return;
            }
            parse_object(a, buf_val, str, pos);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }

            break;
        case '[':
            if (parsing_key) {
                printf("encountered '[' when looking for key");
                v = NULL;
                return;
            }
            parse_array(a, buf_val, str, pos);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }
            break;
        case ',':
            if (parsing_key) {
                printf("encountered ',' when looking for key");
                v = NULL;
                return;
            }
            parsing_value = false;
            parsing_key = true;
            (*pos)++;
            break;
        case '}':
            if (parsing_key) {
                printf("encountered '{' when looking for key");
                v = NULL;
                return;
            }
            if (!parsing_key && !parsing_value) {
                printf("no trailing commas allowed");
                v = NULL;
                return;
            }
            v->u.object.count = kvp_count;
            v->u.object.keys = keys;
            v->u.object.values = values;
            (*pos)++;
            return;
        case ']':
            printf("unexpected character ']' in object parsing");
            v = NULL;
            return;
        default:
            printf("unrecognized token '%c'", str[*pos]);
            (*pos)++;
            break;
    }

    if (buf_val != NULL) {
        values[kvp_count] = buf_val;
        buf_val = NULL;
    }

    goto parse_enter;
}

void parse_array(Arena* a, json_value* v, uint8_t* str, size_t* pos) {
    (*pos)++;  // will skip '['
    v->type = JSON_ARRAY;
    v->u.array.count = 0;

    v->u.array.elements = arena_alloc(a, sizeof(json_value*) * 10);
    uint8_t constant[6] = {0};
    json_value* buf_val = NULL;
    uint8_t* buf_string = NULL;

parse_enter:

    skip_whitespace(str, pos);

    switch (str[*pos]) {
        case '"':
            buf_string = parse_string(a, str, pos);
            if (buf_string == NULL) {
                v = NULL;
                return;
            }

            buf_val = arena_alloc(a, sizeof(json_value));
            buf_val->type = JSON_STRING;
            buf_val->u.string = buf_string;

            buf_string = NULL;
            break;
        case 't':
            buf_val = parse_constant(a, str, pos, "true", 4);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }
            buf_val->type = JSON_BOOL;
            buf_val->u.boolean = true;
            break;
        case 'f':
            buf_val = parse_constant(a, str, pos, "false", 5);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }
            buf_val->type = JSON_BOOL;
            buf_val->u.boolean = false;
            break;
        case 'n':
            buf_val = parse_constant(a, str, pos, "null", 4);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }
            buf_val->type = JSON_NULL;
            break;
        case '{':
            parse_object(a, buf_val, str, pos);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }

            break;
        case '[':
            parse_array(a, buf_val, str, pos);
            if (buf_val == NULL) {
                v = NULL;
                return;
            }

            break;
        case ',':
            (*pos)++;
            break;
        case '}':
            printf("unexpected character '}' in object parsing");
            v = NULL;
            return;
        case ']':
            return;
        default:
            printf("unrecognized token '%c'", str[*pos]);
            (*pos)++;
            break;
    }
    goto parse_enter;
}

json_value* parse_json(uint8_t* str) {
    if (str == NULL) {
        return NULL;
    }

    Arena* a = new_arena(10 * 1024 * 1024);  // 10Mb
    size_t curr_pos = 0;

    json_value* v = arena_alloc(a, sizeof(json_value));

    skip_whitespace(str, &curr_pos);

    if (str[curr_pos] == '{') {
        parse_object(a, v, str, &curr_pos);
    } else if (str[curr_pos] == '[') {
        parse_array(a, v, str, &curr_pos);
    } else {
        printf("json document should start with '{' or '['");
    }

    free_arena(a);

    return v;
}
