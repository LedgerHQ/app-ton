#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "common/mybuffer.h"

// Test buffer_read_bool with valid boolean values
static void test_read_bool_valid(void **state) {
    uint8_t data_true[] = {0x01};
    uint8_t data_false[] = {0x00};
    buffer_t buf;
    bool value;

    buf = (buffer_t){.ptr = data_true, .size = sizeof(data_true), .offset = 0};
    assert_true(buffer_read_bool(&buf, &value));
    assert_true(value == true);
    assert_int_equal(buf.offset, 1);

    buf = (buffer_t){.ptr = data_false, .size = sizeof(data_false), .offset = 0};
    assert_true(buffer_read_bool(&buf, &value));
    assert_true(value == false);
    assert_int_equal(buf.offset, 1);
}

// Test buffer_read_bool with insufficient buffer
static void test_read_bool_insufficient(void **state) {
    uint8_t data[] = {0x01};
    buffer_t buf = {.ptr = data, .size = 1, .offset = 1};
    bool value = true;

    assert_false(buffer_read_bool(&buf, &value));
    assert_false(value);
}

static void test_read_bool_invalid(void **state) {
    uint8_t data_invalid[] = {0x02};
    buffer_t buf = {.ptr = data_invalid, .size = sizeof(data_invalid), .offset = 0};
    bool value;

    assert_false(buffer_read_bool(&buf, &value));
}

// Test buffer_read_u48 with big endian
static void test_read_u48_be(void **state) {
    uint8_t data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xFF};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint64_t value;

    assert_true(buffer_read_u48(&buf, &value, BE));
    assert_int_equal(value, 0x0123456789ABULL);
    assert_int_equal(buf.offset, 6);
}

// Test buffer_read_u48 with little endian
static void test_read_u48_le(void **state) {
    uint8_t data[] = {0xAB, 0x89, 0x67, 0x45, 0x23, 0x01, 0xFF};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint64_t value;

    assert_true(buffer_read_u48(&buf, &value, LE));
    assert_int_equal(value, 0x0123456789ABULL);
    assert_int_equal(buf.offset, 6);
}

// Test buffer_read_u48 with insufficient data
static void test_read_u48_insufficient(void **state) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};  // Only 5 bytes
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint64_t value = 0xFFFFFFFFFFFFFFFFULL;

    assert_false(buffer_read_u48(&buf, &value, BE));
}

// Test buffer_remaining
static void test_remaining(void **state) {
    uint8_t data[10] = {0};
    buffer_t buf = {.ptr = data, .size = 10, .offset = 0};

    assert_int_equal(buffer_remaining(&buf), 10);

    buf.offset = 3;
    assert_int_equal(buffer_remaining(&buf), 7);

    buf.offset = 10;
    assert_int_equal(buffer_remaining(&buf), 0);
}

// Test buffer_read_ref
static void test_read_ref(void **state) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint8_t *ref = NULL;

    assert_true(buffer_read_ref(&buf, &ref, 3));
    assert_ptr_equal(ref, data);  
    assert_int_equal(ref[0], 0x01);
    assert_int_equal(ref[1], 0x02);
    assert_int_equal(ref[2], 0x03);
    assert_int_equal(buf.offset, 3);

    // Read next 2 bytes
    assert_true(buffer_read_ref(&buf, &ref, 2));
    assert_ptr_equal(ref, data + 3);
    assert_int_equal(ref[0], 0x04);
    assert_int_equal(ref[1], 0x05);
    assert_int_equal(buf.offset, 5);
}

// Test buffer_read_ref with insufficient data
static void test_read_ref_insufficient(void **state) {
    uint8_t data[] = {0x01, 0x02};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint8_t *ref = NULL;

    assert_false(buffer_read_ref(&buf, &ref, 5));  // Try to read 5 bytes from 2-byte buffer
}

// Test buffer_read_buffer
static void test_read_buffer(void **state) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint8_t out[3];

    assert_true(buffer_read_buffer(&buf, out, 3));
    assert_int_equal(out[0], 0x01);
    assert_int_equal(out[1], 0x02);
    assert_int_equal(out[2], 0x03);
    assert_int_equal(buf.offset, 3);
}

// Test buffer_read_buffer with insufficient data
static void test_read_buffer_insufficient(void **state) {
    uint8_t data[] = {0x01, 0x02};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint8_t out[5];

    assert_false(buffer_read_buffer(&buf, out, 5));
}

// Test buffer_read_varuint with valid data
static void test_read_varuint_valid(void **state) {
    uint8_t data[] = {0x03, 0xAA, 0xBB, 0xCC, 0xDD};  // length=3, then 3 bytes
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint8_t out[3];
    uint8_t out_size = 0;

    assert_true(buffer_read_varuint(&buf, &out_size, out, sizeof(out)));
    assert_int_equal(out_size, 3);
    assert_int_equal(out[0], 0xAA);
    assert_int_equal(out[1], 0xBB);
    assert_int_equal(out[2], 0xCC);
    assert_int_equal(buf.offset, 4);  // 1 byte for size + 3 bytes data
}

// Test buffer_read_varuint with size exceeding buffer
static void test_read_varuint_size_too_large(void **state) {
    uint8_t data[] = {0x05, 0xAA, 0xBB};  // length=5 but only 2 bytes follow
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint8_t out[5];
    uint8_t out_size = 0;

    assert_false(buffer_read_varuint(&buf, &out_size, out, sizeof(out)));
}

// Test buffer_read_varuint with output buffer too small
static void test_read_varuint_out_too_small(void **state) {
    uint8_t data[] = {0x05, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    uint8_t out[3];  // Too small for 5 bytes
    uint8_t out_size = 0;

    assert_false(buffer_read_varuint(&buf, &out_size, out, sizeof(out)));
}

// Test buffer_read_varuint with no size byte available
static void test_read_varuint_no_size_byte(void **state) {
    uint8_t data[] = {0x01};
    buffer_t buf = {.ptr = data, .size = 1, .offset = 1};
    uint8_t out[5];
    uint8_t out_size = 0;

    assert_false(buffer_read_varuint(&buf, &out_size, out, sizeof(out)));
}

// Test buffer_read_address
static void test_read_address(void **state) {
    uint8_t data[33] = {0xFF};  // chain=0xFF + 32 bytes hash
    for (int i = 1; i < 33; i++) {
        data[i] = i;
    }

    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    address_t addr = {0};

    assert_true(buffer_read_address(&buf, &addr));
    assert_int_equal(addr.chain, 0xFF);
    for (int i = 0; i < 32; i++) {
        assert_int_equal(addr.hash[i], i + 1);
    }
    assert_int_equal(buf.offset, 33);
}

// Test buffer_read_address with insufficient data
static void test_read_address_insufficient(void **state) {
    uint8_t data[0];
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    address_t addr = {0};

    assert_false(buffer_read_address(&buf, &addr));
}

// Test buffer_read_address with chain but no hash
static void test_read_address_partial(void **state) {
    uint8_t data[10] = {0xAA, 0x01, 0x02};
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    address_t addr = {0};

    assert_false(buffer_read_address(&buf, &addr));
}

// Test buffer_read_cell_ref
static void test_read_cell_ref(void **state) {
    uint8_t data[34] = {0x01, 0x23};  // max_depth(BE) + 32 bytes hash
    for (int i = 2; i < 34; i++) {
        data[i] = i - 2;
    }

    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    CellRef_t cell = {0};

    assert_true(buffer_read_cell_ref(&buf, &cell));
    assert_int_equal(cell.max_depth, 0x0123);
    for (int i = 0; i < 32; i++) {
        assert_int_equal(cell.hash[i], i);
    }
    assert_int_equal(buf.offset, 34);
}

// Test buffer_read_cell_ref with insufficient data
static void test_read_cell_ref_insufficient(void **state) {
    uint8_t data[0];
    buffer_t buf = {.ptr = data, .size = sizeof(data), .offset = 0};
    CellRef_t cell = {0};

    assert_false(buffer_read_cell_ref(&buf, &cell));
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_read_bool_valid),
        cmocka_unit_test(test_read_bool_insufficient),
        cmocka_unit_test(test_read_bool_invalid),
        cmocka_unit_test(test_read_u48_be),
        cmocka_unit_test(test_read_u48_le),
        cmocka_unit_test(test_read_u48_insufficient),
        cmocka_unit_test(test_remaining),
        cmocka_unit_test(test_read_ref),
        cmocka_unit_test(test_read_ref_insufficient),
        cmocka_unit_test(test_read_buffer),
        cmocka_unit_test(test_read_buffer_insufficient),
        cmocka_unit_test(test_read_varuint_valid),
        cmocka_unit_test(test_read_varuint_size_too_large),
        cmocka_unit_test(test_read_varuint_out_too_small),
        cmocka_unit_test(test_read_varuint_no_size_byte),
        cmocka_unit_test(test_read_address),
        cmocka_unit_test(test_read_address_insufficient),
        cmocka_unit_test(test_read_address_partial),
        cmocka_unit_test(test_read_cell_ref),
        cmocka_unit_test(test_read_cell_ref_insufficient)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
