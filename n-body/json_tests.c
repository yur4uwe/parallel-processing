#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arena/arena.h"
#include "json/json.h"

#define TEST_ASSERT(cond, msg)                    \
    if (!(cond)) {                                \
        printf("[FAIL] %s: %s\n", __func__, msg); \
        return 0;                                 \
    }

typedef int (*test_func)();

int test_basic_object() {
    Arena* a = new_arena(1024);
    char* json = "{\"key\": 123}";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    TEST_ASSERT(val != NULL, "Failed to parse basic object");
    TEST_ASSERT(val->type == JSON_OBJECT, "Value should be an object");
    TEST_ASSERT(val->u.object.count == 1, "Object should have 1 key");
    TEST_ASSERT(strcmp((char*)val->u.object.keys[0], "key") == 0,
                "Key name mismatch");
    TEST_ASSERT(val->u.object.values[0]->type == JSON_NUMBER,
                "Value type mismatch");
    TEST_ASSERT(val->u.object.values[0]->u.number == 123.0, "Value mismatch");

    free_arena(a);
    return 1;
}

int test_basic_array() {
    Arena* a = new_arena(1024);
    char* json = "[1, true, \"hello\", null]";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    TEST_ASSERT(val != NULL, "Failed to parse basic array");
    TEST_ASSERT(val->type == JSON_ARRAY, "Value should be an array");
    TEST_ASSERT(val->u.array.count == 4, "Array should have 4 elements");
    TEST_ASSERT(val->u.array.elements[0]->type == JSON_NUMBER,
                "Element 0 type mismatch");
    TEST_ASSERT(val->u.array.elements[1]->type == JSON_BOOL,
                "Element 1 type mismatch");
    TEST_ASSERT(val->u.array.elements[1]->u.boolean == true,
                "Element 1 value mismatch");
    TEST_ASSERT(val->u.array.elements[2]->type == JSON_STRING,
                "Element 2 type mismatch");
    TEST_ASSERT(strcmp((char*)val->u.array.elements[2]->u.string, "hello") == 0,
                "Element 2 value mismatch");
    TEST_ASSERT(val->u.array.elements[3]->type == JSON_NULL,
                "Element 3 type mismatch");

    free_arena(a);
    return 1;
}

int test_nested_structure() {
    Arena* a = new_arena(4096);
    char* json = "{\"obj\": {\"a\": [1, 2]}, \"arr\": [{}, 3]}";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    TEST_ASSERT(val != NULL, "Failed to parse nested structure");
    TEST_ASSERT(val->type == JSON_OBJECT, "Should be object");
    TEST_ASSERT(val->u.object.count == 2, "Should have 2 keys");

    // Check "obj"
    json_value* obj_val = NULL;
    for (int i = 0; i < val->u.object.count; i++) {
        if (strcmp((char*)val->u.object.keys[i], "obj") == 0)
            obj_val = val->u.object.values[i];
    }
    TEST_ASSERT(obj_val != NULL && obj_val->type == JSON_OBJECT,
                "Nested object mismatch");

    free_arena(a);
    return 1;
}

int test_malformed_json() {
    Arena* a = new_arena(1024);
    char error_buf[70];
    char* cases[] = {
        "{",         "}",   "[1, 2", "{\"key\": }", "{\"key\" 123}",
        "truefalse", "[,]", NULL};

    for (int i = 0; cases[i] != NULL; i++) {
        json_value* val = parse_json(a, (uint8_t*)cases[i], strlen(cases[i]));
        sprintf(error_buf, "Should have failed on '%s'\n", cases[i]);
        TEST_ASSERT(val == NULL, error_buf);
    }

    free_arena(a);
    return 1;
}

int test_empty_structures() {
    Arena* a = new_arena(1024);

    json_value* obj = parse_json(a, (uint8_t*)"{}", 2);
    TEST_ASSERT(
        obj != NULL && obj->type == JSON_OBJECT && obj->u.object.count == 0,
        "Empty object mismatch");

    json_value* arr = parse_json(a, (uint8_t*)"[]", 2);
    TEST_ASSERT(
        arr != NULL && arr->type == JSON_ARRAY && arr->u.array.count == 0,
        "Empty array mismatch");

    free_arena(a);
    return 1;
}

int test_values() {
    Arena* a = new_arena(1024);

    char* json = "[0, -1, 3.14, true, false, null]";
    json_value* val = parse_json(a, (uint8_t*)json, strlen(json));

    TEST_ASSERT(
        val != NULL && val->type == JSON_ARRAY && val->u.array.count == 6,
        "Failed to parse value array");
    TEST_ASSERT(val->u.array.elements[0]->u.number == 0.0, "Value 0 mismatch");
    TEST_ASSERT(val->u.array.elements[1]->u.number == -1.0,
                "Value -1 mismatch");
    TEST_ASSERT(fabs(val->u.array.elements[2]->u.number - 3.14) < 0.0001,
                "Value 3.14 mismatch");
    TEST_ASSERT(val->u.array.elements[3]->u.boolean == true,
                "Value true mismatch");
    TEST_ASSERT(val->u.array.elements[4]->u.boolean == false,
                "Value false mismatch");
    TEST_ASSERT(val->u.array.elements[5]->type == JSON_NULL,
                "Value null mismatch");

    free_arena(a);
    return 1;
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
