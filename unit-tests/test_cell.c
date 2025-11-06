#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "common/cell.h"
#include "common/bits.h"
#include "mock_cx.h"

// Empty cell should have depth 0
static void test_depth_empty_cell(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t out = {0};

    BitString_init(&bits);
    assert_true(hash_Cell(&bits, NULL, 0, &out));

    // Empty cell with no refs must have depth 0
    assert_int_equal(out.max_depth, 0);
}

// Cell with no refs should have depth 0 regardless of bits
static void test_depth_no_refs(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t out = {0};

    // Cell with 100 bits but no refs
    BitString_init(&bits);
    for (int i = 0; i < 100; i++) {
        BitString_storeBit(&bits, i & 1);
    }

    assert_true(hash_Cell(&bits, NULL, 0, &out));
    assert_int_equal(out.max_depth, 0);
}

// Cell with one ref should have depth = ref_depth + 1
static void test_depth_one_ref(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t refs[1];
    CellRef_t out = {0};

    BitString_init(&bits);
    BitString_storeUint(&bits, 0xAB, 8);

    // Test with ref depth = 5
    refs[0].max_depth = 5;
    memset(refs[0].hash, 0xAA, 32);

    assert_true(hash_Cell(&bits, refs, 1, &out));
    assert_int_equal(out.max_depth, 6);

    // Test with ref depth = 0
    refs[0].max_depth = 0;
    assert_true(hash_Cell(&bits, refs, 1, &out));
    assert_int_equal(out.max_depth, 1);

    // Test with ref depth = 100
    refs[0].max_depth = 100;
    assert_true(hash_Cell(&bits, refs, 1, &out));
    assert_int_equal(out.max_depth, 101);
}

// Cell with multiple refs should have depth = max(ref_depths) + 1
static void test_depth_multiple_refs(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t refs[4];
    CellRef_t out = {0};

    BitString_init(&bits);
    BitString_storeUint(&bits, 0x1234, 16);

    // depths 3, 7, 5 -> max = 7 -> result = 8
    refs[0].max_depth = 3;
    refs[1].max_depth = 7;
    refs[2].max_depth = 5;
    memset(refs[0].hash, 0x11, 32);
    memset(refs[1].hash, 0x22, 32);
    memset(refs[2].hash, 0x33, 32);

    assert_true(hash_Cell(&bits, refs, 3, &out));
    assert_int_equal(out.max_depth, 8);

    // all same depth 10 -> result = 11
    for (int i = 0; i < 3; i++) {
        refs[i].max_depth = 10;
    }
    assert_true(hash_Cell(&bits, refs, 3, &out));
    assert_int_equal(out.max_depth, 11);

    // depths 0, 0, 0, 0 -> result = 1
    for (int i = 0; i < 4; i++) {
        refs[i].max_depth = 0;
        memset(refs[i].hash, 0x10 + i, 32);
    }
    assert_true(hash_Cell(&bits, refs, 4, &out));
    assert_int_equal(out.max_depth, 1);

    // depths 1, 2, 3, 4 -> max = 4 -> result = 5
    for (int i = 0; i < 4; i++) {
        refs[i].max_depth = i + 1;
    }
    assert_true(hash_Cell(&bits, refs, 4, &out));
    assert_int_equal(out.max_depth, 5);
}

// Test with maximum TON cell size (1023 bits)
static void test_max_cell_size(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t out = {0};

    // Create 1023 bits
    BitString_init(&bits);
    for (int i = 0; i < 127; i++) {
        BitString_storeUint(&bits, 0xAA, 8);  // 127 * 8 = 1016 bits
    }
    BitString_storeUint(&bits, 0x7F, 7);  // + 7 bits = 1023 bits

    assert_true(hash_Cell(&bits, NULL, 0, &out));
    assert_int_equal(out.max_depth, 0);
}

// Test with 4 refs
static void test_max_refs(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t refs[4];
    CellRef_t out = {0};

    BitString_init(&bits);
    BitString_storeUint(&bits, 0xFF, 8);

    // Create 4 refs with different depths
    refs[0].max_depth = 10;
    refs[1].max_depth = 20;
    refs[2].max_depth = 15;
    refs[3].max_depth = 25;  // Maximum

    for (int i = 0; i < 4; i++) {
        memset(refs[i].hash, 0x10 + i, 32);
    }

    assert_true(hash_Cell(&bits, refs, 4, &out));
    assert_int_equal(out.max_depth, 26);
}

// Verify hash is written 
static void test_hash_is_computed(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t out = {0};

    BitString_init(&bits);
    BitString_storeUint(&bits, 0xDEADBEEF, 32);

    assert_true(hash_Cell(&bits, NULL, 0, &out));

    // Hash should not be all zeros
    int all_zeros = 1;
    for (int i = 0; i < 32; i++) {
        if (out.hash[i] != 0) {
            all_zeros = 0;
            break;
        }
    }
    assert_false(all_zeros);
}

// Determinism - same input produces same output
static void test_determinism(void **state) {
    (void) state;
    BitString_t bits1, bits2;
    CellRef_t refs1[2], refs2[2];
    CellRef_t out1 = {0}, out2 = {0};

    // Create identical cells
    BitString_init(&bits1);
    BitString_storeUint(&bits1, 0x12345678, 32);
    refs1[0].max_depth = 5;
    refs1[1].max_depth = 3;
    memset(refs1[0].hash, 0xAA, 32);
    memset(refs1[1].hash, 0xBB, 32);

    BitString_init(&bits2);
    BitString_storeUint(&bits2, 0x12345678, 32);
    refs2[0].max_depth = 5;
    refs2[1].max_depth = 3;
    memset(refs2[0].hash, 0xAA, 32);
    memset(refs2[1].hash, 0xBB, 32);

    // Hash both
    assert_true(hash_Cell(&bits1, refs1, 2, &out1));
    assert_true(hash_Cell(&bits2, refs2, 2, &out2));

    // Results must be identical
    assert_int_equal(out1.max_depth, out2.max_depth);
    assert_memory_equal(out1.hash, out2.hash, 32);
}

// Different inputs produce different hashes (with high probability)
static void test_uniqueness(void **state) {
    (void) state;
    BitString_t bits1, bits2;
    CellRef_t out1 = {0}, out2 = {0};

    // Create different cells
    BitString_init(&bits1);
    BitString_storeUint(&bits1, 0xAAAAAAAA, 32);

    BitString_init(&bits2);
    BitString_storeUint(&bits2, 0xBBBBBBBB, 32);

    assert_true(hash_Cell(&bits1, NULL, 0, &out1));
    assert_true(hash_Cell(&bits2, NULL, 0, &out2));

    // Hashes should be different
    int hashes_equal = 1;
    for (int i = 0; i < 32; i++) {
        if (out1.hash[i] != out2.hash[i]) {
            hashes_equal = 0;
            break;
        }
    }
    assert_false(hashes_equal);
}

// Edge case - refs with very large depth values
static void test_large_depth_values(void **state) {
    (void) state;
    BitString_t bits;
    CellRef_t refs[2];
    CellRef_t out = {0};

    BitString_init(&bits);

    // Test with large depth values
    refs[0].max_depth = 1000;
    refs[1].max_depth = 2000;
    memset(refs[0].hash, 0xAA, 32);
    memset(refs[1].hash, 0xBB, 32);

    assert_true(hash_Cell(&bits, refs, 2, &out));
    assert_int_equal(out.max_depth, 2001);  // max(1000, 2000) + 1
}

static void test_descriptor_calculation(void **state) {
    (void) state;
    BitString_t bits1, bits7;
    CellRef_t out1 = {0}, out7 = {0};

    // Test with 1 bit: d2 = (1 >> 3) + ((1 + 7) >> 3) = 0 + 1 = 1
    BitString_init(&bits1);
    BitString_storeBit(&bits1, 1);
    assert_true(hash_Cell(&bits1, NULL, 0, &out1));

    // Test with 7 bits: d2 = (7 >> 3) + ((7 + 7) >> 3) = 0 + 1 = 1
    BitString_init(&bits7);
    BitString_storeUint(&bits7, 0x7F, 7);
    assert_true(hash_Cell(&bits7, NULL, 0, &out7));


    // even though 1-bit and 7-bit have same d2, they have different data so different hash
    int hash1_eq_hash7 = (memcmp(out1.hash, out7.hash, 32) == 0);

    assert_false(hash1_eq_hash7);  // Same d2, different data -> different hash
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_depth_empty_cell),
        cmocka_unit_test(test_depth_no_refs),
        cmocka_unit_test(test_depth_one_ref),
        cmocka_unit_test(test_depth_multiple_refs),
        cmocka_unit_test(test_max_cell_size),
        cmocka_unit_test(test_max_refs),
        cmocka_unit_test(test_hash_is_computed),
        cmocka_unit_test(test_determinism),
        cmocka_unit_test(test_uniqueness),
        cmocka_unit_test(test_large_depth_values),
        cmocka_unit_test(test_descriptor_calculation)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
