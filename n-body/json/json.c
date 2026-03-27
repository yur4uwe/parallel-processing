#include "json.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../arena/arena.h"

enum {
    PARSING_KEY = 0,
    PARSING_VALUE = 1,
    LOOKING_FOR_COMMA = 2,
    LOOKING_FOR_COLON = 3,
};

json_value* parse_array(Arena* a, uint8_t* json_str, size_t* pos,
                        uint64_t str_len);
json_value* parse_object(Arena* a, uint8_t* json_str, size_t* pos,
                         uint64_t str_len);

void skip_whitespace(uint8_t* json_str, size_t* pos) {
    while (isspace(json_str[*pos])) {
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

json_value* parse_string(Arena* a, uint8_t* json_str, size_t* pos) {
    (*pos)++;  // skip '"'
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

    uint8_t* val_str = arena_alloc(a, sizeof(uint8_t) * strlen + 1);

    for (int i = 0; i < strlen; i++) {
        val_str[i] = json_str[*pos - strlen + i];
    }
    val_str[strlen] = '\0';  // null terminate the string

    (*pos)++;  // skip '"'

    json_value* v = arena_alloc(a, sizeof(json_value));
    v->type = JSON_STRING;
    v->u.string = val_str;

    return v;
}

json_value* parse_constant(Arena* a, uint8_t* json_str, size_t* pos,
                           char* expected_constant, int exp_const_len) {
    uint8_t constant[7];  // to fit "false\0"
    strncpy((char*)constant, (char*)(&json_str[*pos]), exp_const_len);
    if (strncmp((char*)constant, expected_constant, exp_const_len) ==
            EXIT_SUCCESS &&
        (isspace(json_str[*pos + exp_const_len]) ||
         json_str[*pos + exp_const_len] == ',' ||
         json_str[*pos + exp_const_len] == '}' ||
         json_str[*pos + exp_const_len] == ']' ||
         json_str[*pos + exp_const_len] == '\0')) {
        json_value* val = arena_alloc(a, sizeof(json_value));
        (*pos) += exp_const_len;  // do not skip ',' for state management sake
        if (strcmp("true", expected_constant) == EXIT_SUCCESS) {
            val->type = JSON_BOOL;
            val->u.boolean = true;
        } else if (strcmp("false", expected_constant) == EXIT_SUCCESS) {
            val->type = JSON_BOOL;
            val->u.boolean = false;
        } else {
            val->type = JSON_NULL;
        }
        return val;
    }
    printf("[ERROR] expected '%s' constant, got %s\n", expected_constant,
           constant);
    return NULL;
}

json_value* parse_number(Arena* a, uint8_t* json_str, size_t* pos) {
    errno = 0;
    char* endptr = NULL;
    double res = strtod((char*)(&json_str[*pos]), &endptr);

    if (errno == ERANGE) {
        printf("WARNING: number exceedes range of double\n");
        res = 0.0;
    }
    size_t num_len = (size_t)((uint8_t*)endptr - &json_str[*pos]);
    if (num_len == 0) {
        printf("no number was detected\n");
        return NULL;
    }
    (*pos) += num_len;

    json_value* v = arena_alloc(a, sizeof(json_value));
    v->type = JSON_NUMBER;
    v->u.number = res;

    return v;
}

json_value* parse_value(Arena* a, uint8_t* json_str, size_t* pos,
                        uint64_t file_size) {
    if (isdigit(json_str[*pos]) || json_str[*pos] == '-') {
        return parse_number(a, json_str, pos);
    }
    switch (json_str[*pos]) {
        case '"':
            return parse_string(a, json_str, pos);
        case '.':
            return parse_number(a, json_str, pos);
        case 't':
            return parse_constant(a, json_str, pos, "true", 4);
        case 'f':
            return parse_constant(a, json_str, pos, "false", 5);
        case 'n':
            return parse_constant(a, json_str, pos, "null", 4);
        case '{':
            return parse_object(a, json_str, pos, file_size);
        case '[':
            return parse_array(a, json_str, pos, file_size);
        default:
            printf("Unexpected token '%c'\n", json_str[*pos]);
            return NULL;
    }
}

void skip_json_value(uint8_t* json_str, size_t* pos, uint64_t str_len);

void skip_json_string(uint8_t* json_str, size_t* pos, uint64_t str_len) {
    (*pos)++; // skip opening quote
    while (*pos < str_len && json_str[*pos] != '"') {
        if (json_str[*pos] == '\\') {
            (*pos)++;
        }
        (*pos)++;
    }
    if (*pos < str_len) (*pos)++; // skip closing quote
}

void skip_json_value(uint8_t* json_str, size_t* pos, uint64_t str_len) {
    skip_whitespace(json_str, pos);
    if (*pos >= str_len) return;

    switch (json_str[*pos]) {
        case '{': {
            (*pos)++;
            int depth = 1;
            while (*pos < str_len && depth > 0) {
                skip_whitespace(json_str, pos);
                if (json_str[*pos] == '"') skip_json_string(json_str, pos, str_len);
                else if (json_str[*pos] == '{') { depth++; (*pos)++; }
                else if (json_str[*pos] == '}') { depth--; (*pos)++; }
                else (*pos)++;
            }
            break;
        }
        case '[': {
            (*pos)++;
            int depth = 1;
            while (*pos < str_len && depth > 0) {
                skip_whitespace(json_str, pos);
                if (json_str[*pos] == '"') skip_json_string(json_str, pos, str_len);
                else if (json_str[*pos] == '[') { depth++; (*pos)++; }
                else if (json_str[*pos] == ']') { depth--; (*pos)++; }
                else (*pos)++;
            }
            break;
        }
        case '"':
            skip_json_string(json_str, pos, str_len);
            break;
        default:
            while (*pos < str_len && !isspace(json_str[*pos]) && json_str[*pos] != ',' && json_str[*pos] != '}' && json_str[*pos] != ']') {
                (*pos)++;
            }
            break;
    }
}

int count_object_elements(uint8_t* json_str, size_t pos, uint64_t str_len) {
    if (json_str[pos] != '{') return 0;
    pos++;
    skip_whitespace(json_str, &pos);
    if (json_str[pos] == '}') return 0;

    int count = 0;
    while (pos < str_len && json_str[pos] != '}') {
        count++;
        skip_json_value(json_str, &pos, str_len); // skip key
        skip_whitespace(json_str, &pos);
        if (json_str[pos] == ':') pos++;
        skip_json_value(json_str, &pos, str_len); // skip value
        skip_whitespace(json_str, &pos);
        if (json_str[pos] == ',') pos++;
        skip_whitespace(json_str, &pos);
    }
    return count;
}

int count_array_elements(uint8_t* json_str, size_t pos, uint64_t str_len) {
    if (json_str[pos] != '[') return 0;
    pos++;
    skip_whitespace(json_str, &pos);
    if (json_str[pos] == ']') return 0;

    int count = 0;
    while (pos < str_len && json_str[pos] != ']') {
        count++;
        skip_json_value(json_str, &pos, str_len);
        skip_whitespace(json_str, &pos);
        if (json_str[pos] == ',') pos++;
        skip_whitespace(json_str, &pos);
    }
    return count;
}

json_value* parse_object(Arena* a, uint8_t* json_str, size_t* pos,
                         uint64_t str_len) {
    int total_count = count_object_elements(json_str, *pos, str_len);
    (*pos)++;  // will skip '{'
    json_value* v = arena_alloc(a, sizeof(json_value));
    v->type = JSON_OBJECT;
    v->u.object.count = 0;
    v->u.object.keys = arena_alloc(a, sizeof(char*) * total_count);
    v->u.object.values = arena_alloc(a, sizeof(json_value*) * total_count);

    int state = PARSING_KEY;

    json_value* buf_val = NULL;

parse_enter:
    if (*pos >= str_len) {
        printf("exceeded json string length\n");
        return NULL;
    }

    skip_whitespace(json_str, pos);

    if (state == LOOKING_FOR_COLON && json_str[*pos] == ':') {
        state = PARSING_VALUE;
        (*pos)++;
        goto parse_enter;
    } else if (state == LOOKING_FOR_COMMA && json_str[*pos] == ',') {
        state = PARSING_KEY;
        (*pos)++;
        goto parse_enter;
    } else if (state == LOOKING_FOR_COMMA && json_str[*pos] == '}') {
        (*pos)++;
        return v;
    }

    if (state == PARSING_KEY) {
        if (json_str[*pos] != '"') {
            if (json_str[*pos] == '}') {
                (*pos)++;
                return v;
            }
            printf(
                "Expected to find '\"' while looking for a key, found %c "
                "instead\n",
                json_str[*pos]);
            return NULL;
        }
        buf_val = parse_string(a, json_str, pos);
        if (buf_val == NULL) {
            return NULL;
        }
        v->u.object.keys[v->u.object.count++] = buf_val->u.string;
        state = LOOKING_FOR_COLON;
        goto parse_enter;
    }

    if (state != PARSING_VALUE) {
        printf("incorrect state, got %d, expected PARSING_VALUE(%d), active char: '%c'\n", state,
               PARSING_VALUE, json_str[*pos]);
        return NULL;
    }

    // switch for parsing value
    buf_val = parse_value(a, json_str, pos, str_len);
    if (buf_val == NULL) {
        return NULL;
    }
    v->u.object.values[v->u.object.count - 1] = buf_val;
    buf_val = NULL;
    state = LOOKING_FOR_COMMA;

    goto parse_enter;
}

json_value* parse_array(Arena* a, uint8_t* json_str, size_t* pos,
                        uint64_t str_len) {
    int total_count = count_array_elements(json_str, *pos, str_len);
    (*pos)++;  // will skip '['
    json_value* v = arena_alloc(a, sizeof(json_value));
    v->type = JSON_ARRAY;
    v->u.array.count = 0;
    v->u.array.elements = arena_alloc(a, sizeof(json_value*) * total_count);

    int state = PARSING_VALUE;

    json_value* buf_val = NULL;

parse_enter:
    if (*pos >= str_len) {
        printf("exceeded json string length\n");
        return NULL;
    }

    skip_whitespace(json_str, pos);

    if (json_str[*pos] == ']') {
        (*pos)++;
        return v;
    }

    if (state == LOOKING_FOR_COMMA && json_str[*pos] == ',') {
        state = PARSING_VALUE;
        (*pos)++;
        goto parse_enter;
    } else if (state == LOOKING_FOR_COMMA && json_str[*pos] == ']') {
        (*pos)++;
        return v;
    }

    if (state != PARSING_VALUE) {
        if (json_str[*pos] == ']') {
            (*pos)++;
            return v;
        }
        printf("Expected state PARSING_VALUE(%d), got %d\n", PARSING_VALUE,
               state);
        return NULL;
    }

    buf_val = parse_value(a, json_str, pos, str_len);
    if (buf_val == NULL) {
        return NULL;
    }

    v->u.array.elements[v->u.array.count] = buf_val;
    v->u.array.count++;
    buf_val = NULL;
    state = LOOKING_FOR_COMMA;

    goto parse_enter;
}

json_value* parse_json(Arena* a, uint8_t* json_str, uint64_t str_len) {
    if (json_str == NULL) {
        return NULL;
    }

    size_t curr_pos = 0;

    skip_whitespace(json_str, &curr_pos);

    if (json_str[curr_pos] == '{') {
        return parse_object(a, json_str, &curr_pos, str_len);
    } else if (json_str[curr_pos] == '[') {
        return parse_array(a, json_str, &curr_pos, str_len);
    } else {
        printf("json document should start with '{' or '['\n");
    }

    return NULL;
}
