#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "json/json.h"

typedef int (*test_func)();

int test_basic_object() {
    Arena* a = new_arena(1024);
    int ec = EXIT_SUCCESS;
    char* json = "{\"key\": 123}";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    JSON_ASSERT_OBJECT(val, 1, "Should be object with 1 key");
    JSON_ASSERT_NAME(val->u.object.keys[0], "key",
                "Key name mismatch");
    JSON_ASSERT_NUMBER(val->u.object.values[0], 123.0, "Value mismatch");

    free_arena(a);
    return 1;

assert_failed:
    free_arena(a);
    return 0;
}

int test_basic_array() {
    Arena* a = new_arena(1024);
    int ec = EXIT_SUCCESS;
    char* json = "[1, true, \"hello\", null]";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    JSON_ASSERT_ARRAY(val, 4, "Should be array with 4 elements");
    JSON_ASSERT_NUMBER(val->u.array.elements[0], 1.0, "Element 0 mismatch");
    JSON_ASSERT_BOOL(val->u.array.elements[1], true, "Element 1 mismatch");
    JSON_ASSERT_STRING(val->u.array.elements[2], "hello", "Element 2 mismatch");
    JSON_ASSERT_NULL(val->u.array.elements[3], "Element 3 should be null");

    free_arena(a);
    return 1;

assert_failed:
    free_arena(a);
    return 0;
}

int test_nested_structure() {
    Arena* a = new_arena(4096);
    int ec = EXIT_SUCCESS;
    char* json = "{\"obj\": {\"a\": [1, 2]}, \"arr\": [{}, 3]}";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    JSON_ASSERT_OBJECT(val, 2, "Should have 2 keys");

    // Check "obj"
    json_value* obj_val = NULL;
    for (int i = 0; i < val->u.object.count; i++) {
        if (strcmp((char*)val->u.object.keys[i], "obj") == 0)
            obj_val = val->u.object.values[i];
    }
    JSON_ASSERT_OBJECT(obj_val, 1, "Nested object mismatch");

    free_arena(a);
    return 1;

assert_failed:
    free_arena(a);
    return 0;
}

int test_malformed_json() {
    Arena* a = new_arena(1024);
    char error_buf[70];
    char* cases[] = {"{",
                     "}",
                     "[1, 2",
                     "{\"key\": }",
                     "{\"key\" 123}",
                     "truefalse",
                     "[,]",
                     "{\"key\": 123, \"key\": 123}",
                     NULL};

    for (int i = 0; cases[i] != NULL; i++) {
        json_value* val = parse_json(a, (uint8_t*)cases[i], strlen(cases[i]));
        sprintf(error_buf, "Should have failed on '%s'", cases[i]);
        if (val != NULL) {
            printf("[FAIL] %s: %s\n", __func__, error_buf);
            free_arena(a);
            return 0;
        }
    }

    free_arena(a);
    return 1;
}

int test_empty_structures() {
    Arena* a = new_arena(1024);
    int ec = EXIT_SUCCESS;

    json_value* obj = parse_json(a, (uint8_t*)"{}", 2);
    JSON_ASSERT_OBJECT(obj, 0, "Empty object mismatch");

    json_value* arr = parse_json(a, (uint8_t*)"[]", 2);
    JSON_ASSERT_ARRAY(arr, 0, "Empty array mismatch");

    free_arena(a);
    return 1;

assert_failed:
    free_arena(a);
    return 0;
}

int test_values() {
    Arena* a = new_arena(1024);
    int ec = EXIT_SUCCESS;

    char* json = "[0, -1, 3.14, true, false, null]";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    JSON_ASSERT_ARRAY(val, 6, "Failed to parse value array");
    JSON_ASSERT_NUMBER(val->u.array.elements[0], 0.0, "Value 0 mismatch");
    JSON_ASSERT_NUMBER(val->u.array.elements[1], -1.0, "Value -1 mismatch");
    JSON_ASSERT_NUMBER(val->u.array.elements[2], 3.14, "Value 3.14 mismatch");
    JSON_ASSERT_BOOL(val->u.array.elements[3], true, "Value true mismatch");
    JSON_ASSERT_BOOL(val->u.array.elements[4], false, "Value false mismatch");
    JSON_ASSERT_NULL(val->u.array.elements[5], "Value null mismatch");

    free_arena(a);
    return 1;

assert_failed:
    free_arena(a);
    return 0;
}

int main() {
    test_func tests[] = {test_basic_object,
                         test_basic_array,
                         test_nested_structure,
                         test_malformed_json,
                         test_empty_structures,
                         test_values,
                         NULL};

    int passed = 0;
    int total = 0;

    for (int i = 0; tests[i] != NULL; i++) {
        total++;
        if (tests[i]()) {
            passed++;
        }
    }

    printf("\nTest Summary: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
