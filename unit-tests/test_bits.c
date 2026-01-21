#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "common/bits.h"

static void test_bits(void **state) {
    uint8_t expected[21] = {0xe3, 0x79, 0x2f, 0x59, 0x01, 0x0a, 0x30, 0xf4, 0x24, 0x05, 0x02,
                            0x54, 0x0b, 0xe4, 0x00, 0x50, 0x2d, 0xdd, 0xef, 0xa0, 0x38};

    BitString_t bits;
    BitString_init(&bits);

    // Test bits writes
    BitString_storeBit(&bits, 1);
    assert_true(bits.data_cursor == 1);
    BitString_storeBit(&bits, 2);
    assert_true(bits.data_cursor == 2);
    BitString_storeBit(&bits, 3);
    assert_true(bits.data_cursor == 3);
    BitString_storeBit(&bits, 0);
    assert_true(bits.data_cursor == 4);
    BitString_storeBit(&bits, -1);
    assert_true(bits.data_cursor == 5);
    BitString_storeBit(&bits, -2);
    assert_true(bits.data_cursor == 6);
    BitString_storeBit(&bits, 1);
    assert_true(bits.data_cursor == 7);
    BitString_storeBit(&bits, 1);
    assert_true(bits.data_cursor == 8);
    assert_int_equal(bits.data[0], 0xe3);

    // Test uint writes
    BitString_storeUint(&bits, 121, 8);
    BitString_storeUint(&bits, 12121, 16);
    assert_int_equal(bits.data[1], 0x79);
    assert_int_equal(bits.data[2], 0x2f);
    assert_int_equal(bits.data[3], 0x59);

    // Test coins writes
    // 010a30f4240502540be400
    BitString_storeCoins(&bits, 0);
    assert_true(bits.data_cursor == 36);
    BitString_storeCoins(&bits, 10);
    assert_true(bits.data_cursor == 48);
    BitString_storeCoins(&bits, 1000000);
    assert_true(bits.data_cursor == 76);
    BitString_storeCoins(&bits, 10000000000);
    assert_true(bits.data_cursor == 120);
    BitString_storeCoins(&bits, 12312312323);
    assert_true(bits.data_cursor == 164);

    // Finalize
    BitString_finalize(&bits);

    // Contents and hash
    assert_memory_equal(bits.data, expected, sizeof(expected));
}

static void test_bits_2(void **state) {
    uint8_t expected[1] = {0x20};
    BitString_t bits;
    BitString_init(&bits);
    BitString_storeBit(&bits, 0);
    BitString_storeBit(&bits, 0);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected, sizeof(expected));
}

static void test_coins_buf(void **state) {
    uint8_t expected[8] = {0x07, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xde};
    BitString_t bits;
    BitString_init(&bits);
    BitString_storeUint(&bits, 0, 4);  // align the coins
    uint8_t coins[7] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xde};
    BitString_storeCoinsBuf(&bits, coins, sizeof(coins));
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8 * sizeof(expected));
    assert_memory_equal(bits.data, expected, sizeof(expected));
}

static void test_null_addr(void **state) {
    uint8_t expected[1] = {0x20};
    BitString_t bits;
    BitString_init(&bits);
    BitString_storeAddressNull(&bits);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected, sizeof(expected));
}

static void test_addr(void **state) {
    uint8_t expected[34] = {0};
    expected[0] = 0x80;
    expected[33] = 0x10;
    uint8_t hash[32] = {0};
    BitString_t bits;
    BitString_init(&bits);
    BitString_storeAddress(&bits, 0x00, hash);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8 * sizeof(expected));
    assert_memory_equal(bits.data, expected, sizeof(expected));
}

static void test_finalize_padding(void **state) {
    BitString_t bits;

    // Case 0
    // no padding needed
    uint8_t expected_0[1] = {0x00};
    BitString_init(&bits);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 0);
    assert_memory_equal(bits.data, expected_0, 1);

    // Case 1
    // Bit pattern: 1 + (padding: 1 000000) = 1100 0000 = 0xC0
    uint8_t expected_1[1] = {0xC0};
    BitString_init(&bits);
    BitString_storeBit(&bits, 1);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_1, sizeof(expected_1));

    // Case 2
    // Bit pattern: 10 + (padding: 1 00000) = 1010 0000 = 0xA0
    uint8_t expected_2[1] = {0xA0};
    BitString_init(&bits);
    BitString_storeBit(&bits, 1);
    BitString_storeBit(&bits, 0);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_2, sizeof(expected_2));

    // Case 3
    // Bit pattern: 101 + (padding: 1 0000) = 1011 0000 = 0xB0
    uint8_t expected_3[1] = {0xB0};
    BitString_init(&bits);
    BitString_storeBit(&bits, 1);
    BitString_storeBit(&bits, 0);
    BitString_storeBit(&bits, 1);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_3, sizeof(expected_3));

    // Case 4
    // Bit pattern: 1010 + (padding: 1 000) = 1010 1000 = 0xA8
    uint8_t expected_4[1] = {0xA8};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0xA, 4);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_4, sizeof(expected_4));

    // Case 5
    // Bit pattern: 10101 + (padding: 1 00) = 1010 1100 = 0xAC
    uint8_t expected_5[1] = {0xAC};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0x15, 5);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_5, sizeof(expected_5));

    // Case 6
    // Bit pattern: 101010 + (padding: 1 0) = 1010 1010 = 0xAA
    uint8_t expected_6[1] = {0xAA};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0x2A, 6);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_6, sizeof(expected_6));

    // Case 7
    // Bit pattern: 1010101 + (padding: 1) = 1010 1011 = 0xAB
    uint8_t expected_7[1] = {0xAB};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0x55, 7);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_7, sizeof(expected_7));

    // Case 8
    // no padding needed
    uint8_t expected_8[1] = {0xFF};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0xFF, 8);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_8, sizeof(expected_8));
}

static void test_init_clears_memory(void **state) {
    BitString_t bits;

    // Fill with garbage
    memset(&bits, 0xFF, sizeof(bits));

    // Init should clear everything
    BitString_init(&bits);

    assert_int_equal(bits.data_cursor, 0);
    for (int i = 0; i < sizeof(bits.data); i++) {
        assert_int_equal(bits.data[i], 0);
    }
}

static void test_storeUint_bits_greater_than_64(void **state) {
    BitString_t bits;

    // Test case: bits = 65, value = 0xFF
    // Should store 1 zero bit, then 64 bits of 0xFF
    // Expected: 72 bits total = 1 zero bits followed by 64 bits of 0xFF, then finalize adds: 1 bit
    // + 0s 0111 1111 | 1111 1111 | ... | 1111 1111 | 1100 0000
    uint8_t expected_65[9] = {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0xFFFFFFFFFFFFFFFFULL, 65);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 72);  // 65 + 7 bits padding = 72
    assert_memory_equal(bits.data, expected_65, sizeof(expected_65));

    // Test case: bits = 70, value = 0xFFFFFFFFFFFFFFFF
    // Expected: 72 bits total = 6 zero bits followed by 64 bits of 0xFF, then finalize adds: 1 bit
    // + 0s 0000 0011 | 1111 1111 | ... | 1111 1111 | 1111 1110
    uint8_t expected_70[9] = {0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0xFFFFFFFFFFFFFFFFULL, 70);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 72);  // 70 + 2 bits padding = 72
    assert_memory_equal(bits.data, expected_70, sizeof(expected_70));

    // Test case: bits = 72, value = 0x123456789ABCDEF0
    // Should store 8 zero bits, then 64 bits of actual value
    uint8_t expected_72[9] = {0x00, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0x123456789ABCDEF0ULL, 72);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 72);
    assert_memory_equal(bits.data, expected_72, sizeof(expected_72));
}

static void test_storeUint_64_bits(void **state) {
    BitString_t bits;

    // Test case: bits = 64, value = 0xFFFFFFFFFFFFFFFF
    uint8_t expected_max[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0xFFFFFFFFFFFFFFFFULL, 64);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 64);
    assert_memory_equal(bits.data, expected_max, sizeof(expected_max));

    // Test case: bits = 64, value = 0x123456789ABCDEF0
    uint8_t expected_val[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0x123456789ABCDEF0ULL, 64);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 64);
    assert_memory_equal(bits.data, expected_val, sizeof(expected_val));

    // Test case: bits = 64, value = 0 (all zeros)
    uint8_t expected_zero[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0, 64);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 64);
    assert_memory_equal(bits.data, expected_zero, sizeof(expected_zero));
}

static void test_storeUint_various_cases(void **state) {
    BitString_t bits;

    // Test case: bits = 1, value = 1
    uint8_t expected_1bit[1] = {0xC0};  // 1 + padding (100000) = 11000000
    BitString_init(&bits);
    BitString_storeUint(&bits, 1, 1);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_1bit, sizeof(expected_1bit));

    // Test case: bits = 4, value = 15 (0xF)
    uint8_t expected_4bits[1] = {0xF8};  // 1111 + padding (1000) = 11111000
    BitString_init(&bits);
    BitString_storeUint(&bits, 15, 4);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_4bits, sizeof(expected_4bits));

    // Test case: bits = 32, value = 0x12345678
    uint8_t expected_32bits[4] = {0x12, 0x34, 0x56, 0x78};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0x12345678, 32);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 32);
    assert_memory_equal(bits.data, expected_32bits, sizeof(expected_32bits));

    // Test case: Value larger than bits can represent (should truncate high bits)
    // bits = 8, value = 0x1FF (9 bits set) -> should only store lower 8 bits (0xFF)
    uint8_t expected_truncate[1] = {0xFF};
    BitString_init(&bits);
    BitString_storeUint(&bits, 0x1FF, 8);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8);
    assert_memory_equal(bits.data, expected_truncate, sizeof(expected_truncate));
}

static void test_storeBit_edge_cases(void **state) {
    BitString_t bits;

    BitString_init(&bits);
    for (int i = 0; i < 16; i++) {
        BitString_storeBit(&bits, i % 2);
    }

    // 01010101 01010101 = 0x55 0x55
    assert_int_equal(bits.data[0], 0x55);
    assert_int_equal(bits.data[1], 0x55);
    assert_int_equal(bits.data_cursor, 16);

    // Test storing all zeros
    BitString_init(&bits);
    for (int i = 0; i < 8; i++) {
        BitString_storeBit(&bits, 0);
    }
    assert_int_equal(bits.data[0], 0x00);
    assert_int_equal(bits.data_cursor, 8);

    // Test storing all ones
    BitString_init(&bits);
    for (int i = 0; i < 8; i++) {
        BitString_storeBit(&bits, 1);
    }
    assert_int_equal(bits.data[0], 0xFF);
    assert_int_equal(bits.data_cursor, 8);

    BitString_init(&bits);
    BitString_storeBit(&bits, -100);  // 0
    BitString_storeBit(&bits, -1);    // 0
    BitString_storeBit(&bits, -128);  // 0
    BitString_storeBit(&bits, 1);     // 1
    BitString_storeBit(&bits, 100);   // 1
    BitString_storeBit(&bits, 0);     // 0
    BitString_storeBit(&bits, 0);     // 0
    BitString_storeBit(&bits, 1);     // 1
    // 00011001 = 0x19
    assert_int_equal(bits.data[0], 0x19);
}

static void test_storeBuffer_various_sizes(void **state) {
    BitString_t bits;

    // Test single byte
    BitString_init(&bits);
    uint8_t single[1] = {0xAB};
    BitString_storeBuffer(&bits, single, 1);
    assert_int_equal(bits.data_cursor, 8);
    assert_int_equal(bits.data[0], 0xAB);

    // Test multiple bytes
    BitString_init(&bits);
    uint8_t multi[5] = {0x12, 0x34, 0x56, 0x78, 0x9A};
    BitString_storeBuffer(&bits, multi, 5);
    assert_int_equal(bits.data_cursor, 40);
    assert_memory_equal(bits.data, multi, 5);

    // Test buffer with all zeros
    BitString_init(&bits);
    uint8_t zeros[4] = {0x00, 0x00, 0x00, 0x00};
    BitString_storeBuffer(&bits, zeros, 4);
    assert_int_equal(bits.data_cursor, 32);
    assert_memory_equal(bits.data, zeros, 4);

    // Test buffer with all ones
    BitString_init(&bits);
    uint8_t ones[3] = {0xFF, 0xFF, 0xFF};
    BitString_storeBuffer(&bits, ones, 3);
    assert_int_equal(bits.data_cursor, 24);
    assert_memory_equal(bits.data, ones, 3);
}

static void test_storeCoins_edge_cases(void **state) {
    BitString_t bits;

    // Test maximum uint64_t value
    BitString_init(&bits);
    BitString_storeCoins(&bits, 0xFFFFFFFFFFFFFFFFULL);
    assert_int_equal(bits.data_cursor, 68);
    uint8_t expected_1val[9] = {0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0};
    assert_memory_equal(bits.data, expected_1val, sizeof(expected_1val));

    BitString_init(&bits);
    BitString_storeCoins(&bits, 256);        // 0x100, needs 2 bytes
    assert_int_equal(bits.data_cursor, 20);  // 4 + 16
    uint8_t expected_2val[3] = {0x20, 0x10, 0x0};
    assert_memory_equal(bits.data, expected_2val, sizeof(expected_2val));

    BitString_init(&bits);
    BitString_storeCoins(&bits, 65536);      // 0x10000, needs 3 bytes
    assert_int_equal(bits.data_cursor, 28);  // 4 + 24
    uint8_t expected_3val[4] = {0x30, 0x10, 0x0, 0x0};
    assert_memory_equal(bits.data, expected_3val, sizeof(expected_3val));

    BitString_init(&bits);
    BitString_storeCoins(&bits, 255);        // 0xFF, needs 1 byte
    assert_int_equal(bits.data_cursor, 12);  // 4 + 8
    uint8_t expected_4val[2] = {0x1F, 0xF0};
    assert_memory_equal(bits.data, expected_4val, sizeof(expected_4val));

    BitString_init(&bits);
    BitString_storeCoins(&bits, 1);
    assert_int_equal(bits.data_cursor, 12);  // 4 + 8
    uint8_t expected_5val[2] = {0x10, 0x10};
    assert_memory_equal(bits.data, expected_5val, sizeof(expected_5val));
}

static void test_storeCoinsBuf_edge_cases(void **state) {
    BitString_t bits;

    // Test with zero length
    BitString_init(&bits);
    uint8_t buf[1] = {0xFF};
    BitString_storeCoinsBuf(&bits, buf, 0);
    BitString_finalize(&bits);
    // Only length stored (4 bits = 0000) + padding (1000)
    // 0000 1000 = 0x08
    assert_int_equal(bits.data_cursor, 8);
    assert_int_equal(bits.data[0], 0x08);

    // Test with max length (15 = 0xF, fits in 4 bits)
    BitString_init(&bits);
    uint8_t max_buf[15] =
        {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    BitString_storeCoinsBuf(&bits, max_buf, 15);
    // 4 bits (length) + 15*8 bits (data) = 124 bits
    assert_int_equal(bits.data_cursor, 124);

    // Test with single byte
    BitString_init(&bits);
    uint8_t single[1] = {0xAB};
    BitString_storeCoinsBuf(&bits, single, 1);
    assert_int_equal(bits.data_cursor, 12);  // 4 + 8
}

static void test_storeAddress_various_cases(void **state) {
    BitString_t bits;

    uint8_t hash1[32];
    memset(hash1, 0xFF, 32);
    BitString_init(&bits);
    BitString_storeAddress(&bits, 0xFF, hash1);
    BitString_finalize(&bits);
    // Address: 2 bits (10) + 1 bit (0) + 8 bits (chain) + 256 bits (hash) = 267 bits
    // After finalize: 267 + 5 bits padding = 272 bits = 34 bytes
    assert_int_equal(bits.data_cursor, 8 * 34);
    // Layout: 10 0 11111111 | 11111111... (32 bytes) | padding
    // Byte 0: 100 11111 = 0x9F
    // Bytes 1-32: all 0xFF
    // Byte 33: 111 10000 = 0xF0
    uint8_t expected1[34];
    expected1[0] = 0x9F;
    memset(&expected1[1], 0xFF, 32);
    expected1[33] = 0xF0;
    assert_memory_equal(bits.data, expected1, sizeof(expected1));

    // Test case 2: chain=0x01, hash=0x00
    uint8_t hash3[32];
    memset(hash3, 0x00, 32);
    BitString_init(&bits);
    BitString_storeAddress(&bits, 0x01, hash3);
    BitString_finalize(&bits);
    assert_int_equal(bits.data_cursor, 8 * 34);
    // Layout: 10 0 00000000 | 00000000... (32 bytes)
    // Byte 0: 100 00000 = 0x80
    // Byte 1: 0010 0000 = 0x20
    // Bytes 2-32: all 0x00
    // Byte 33: 000 10000 = 0x10 (padding)
    uint8_t expected3[34];
    expected3[0] = 0x80;
    expected3[1] = 0x20;
    memset(&expected3[2], 0x00, 31);
    expected3[33] = 0x10;
    assert_memory_equal(bits.data, expected3, sizeof(expected3));
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bits),
        cmocka_unit_test(test_bits_2),
        cmocka_unit_test(test_coins_buf),
        cmocka_unit_test(test_null_addr),
        cmocka_unit_test(test_addr),
        cmocka_unit_test(test_finalize_padding),
        cmocka_unit_test(test_init_clears_memory),

        cmocka_unit_test(test_storeUint_bits_greater_than_64),
        cmocka_unit_test(test_storeUint_64_bits),
        cmocka_unit_test(test_storeUint_various_cases),

        cmocka_unit_test(test_storeBit_edge_cases),
        cmocka_unit_test(test_storeBuffer_various_sizes),
        cmocka_unit_test(test_storeCoins_edge_cases),
        cmocka_unit_test(test_storeCoinsBuf_edge_cases),
        cmocka_unit_test(test_storeAddress_various_cases),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
