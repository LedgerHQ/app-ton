#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Mock types for Ledger crypto library
typedef uint32_t cx_err_t;

// Error codes
#define CX_OK             0x00000000
#define CX_INTERNAL_ERROR 0xFFFFFF85

// Hash flags
#define CX_LAST (1 << 0)

typedef struct {
    uint8_t buffer[512];
    size_t offset;
} cx_sha256_t;

typedef cx_sha256_t cx_hash_t;

/**
 * @brief   Initializes a SHA-256 context.
 *
 * @param[out] hash Pointer to the context.
 *                  The context shall be in RAM.
 *
 * @return          Error code:
 *                  - CX_OK on success
 */
// No need to add WARN_UNUSED_RESULT to cx_sha256_init_no_throw(), it always returns CX_OK
cx_err_t cx_sha256_init_no_throw(cx_sha256_t *hash);

/**
 * @brief   Hashes data according to the specified algorithm.
 *
 * @param[in]  hash    Pointer to the hash context.
 *                     Shall be in RAM.
 *                     Should be called with a cast.
 *
 * @param[in]  mode    Crypto flag. Supported flag: CX_LAST. If set:
 *                       - the structure is not modified after finishing
 *                       - if out is not NULL, the message digest is stored in out
 *                       - the context is NOT automatically re-initialized.
 *
 * @param[in]  in      Input data to be hashed.
 *
 * @param[in]  len     Length of the input data.
 *
 * @param[out] out     Buffer where to store the message digest:
 *                       - NULL (ignored) if CX_LAST is NOT set
 *                       - message digest if CX_LAST is set
 *
 * @param[out] out_len The size of the output buffer or 0 if out is NULL.
 *                     If buffer is too small to store the hash an error is returned.
 *
 * @return             Error code:
 *                     - CX_OK on success
 *                     - INVALID_PARAMETER
 *                     - CX_INVALID_PARAMETER
 */
cx_err_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t len,
                          uint8_t *out,
                          size_t out_len);

// Simple hash function for testing
static void simple_hash_256(const uint8_t *data, size_t len, uint8_t *hash) {
    memset(hash, 0, 32);

    for (size_t i = 0; i < len; i++) {
        hash[i % 32] ^= data[i];
        hash[(i + 1) % 32] = (hash[(i + 1) % 32] + data[i]) & 0xFF;
        hash[(i + 7) % 32] = (hash[(i + 7) % 32] ^ (data[i] << 1)) & 0xFF;
    }

    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 32; i++) {
            hash[i] ^= hash[(i + 13) % 32];
            hash[i] = (hash[i] + 1) & 0xFF;
        }
    }
}

cx_err_t cx_sha256_init_no_throw(cx_sha256_t *hash) {
    hash->offset = 0;
    memset(hash->buffer, 0, sizeof(hash->buffer));
    return CX_OK;
}

cx_err_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t len,
                          uint8_t *out,
                          size_t out_len) {
    if (in != NULL && len > 0) {
        size_t copy_len = len;
        if (hash->offset + copy_len > sizeof(hash->buffer)) {
            copy_len = sizeof(hash->buffer) - hash->offset;
        }
        memcpy(hash->buffer + hash->offset, in, copy_len);
        hash->offset += copy_len;
    }

    if (mode & CX_LAST) {
        simple_hash_256(hash->buffer, hash->offset, out);
    }

    return CX_OK;
}