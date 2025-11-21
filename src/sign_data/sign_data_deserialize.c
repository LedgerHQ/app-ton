#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "cx.h"

#include "../common/mybuffer.h"
#include "../common/encoding.h"
#include "../common/bits.h"
#include "../common/cell.h"
#include "../common/types.h"

#include "../address.h"
#include "../constants.h"
#include "../types.h"
#include "../apdu/params.h"

#define PLAINTEXT_REQUEST          0x754bf91b
#define APP_DATA_REQUEST           0x54b58535

#define TEXT_TYPE_ID               0x00
#define BINARY_TYPE_ID             0x01
#define CELL_TYPE_ID               0x02

#define MAX_PLAINTEXT_LENGTH       120
#define MAX_APP_DATA_DOMAIN_LENGTH 126  // max allowed domain len as per TON DNS spec

#define SAFE(RES)     \
    if (!RES) {       \
        return false; \
    }

static const uint8_t TON_CONNECT_SIGN_DATA_STR[] = "\xff\xffton-connect/sign-data/";
static const uint8_t SIGN_DATA_TXT_PREFIX[] = "txt";
static const uint8_t SIGN_DATA_BIN_PREFIX[] = "bin";

int roffset(uint8_t* data, size_t data_len, uint8_t c) {
    for (int i = data_len - 1; i >= 0; i--) {
        if (data[i] == c) {
            return i;
        }
    }
    return -1;
}

int encode_domain_buf(uint8_t* domain, size_t domain_len, uint8_t* buf, size_t buf_len) {
    size_t cur_len = domain_len;
    int doffset;
    int encoded = 0;
    while ((doffset = roffset(domain, cur_len, '.')) >= 0) {
        if (buf_len < cur_len - doffset) {
            return -1;
        }

        memmove(buf, &domain[doffset + 1], cur_len - doffset - 1);
        buf[cur_len - doffset - 1] = 0;
        buf += cur_len - doffset;
        buf_len -= cur_len - doffset;
        encoded += cur_len - doffset;

        cur_len = doffset;
    }
    if (buf_len < cur_len + 1) {
        return -1;
    }
    memmove(buf, domain, cur_len);
    buf[cur_len] = 0;
    encoded += cur_len + 1;
    return encoded;
}

void encode_domain(BitString_t* self, uint8_t* domain, size_t domain_len) {
    size_t cur_len = domain_len;
    int doffset;
    while ((doffset = roffset(domain, cur_len, '.')) >= 0) {
        BitString_storeBuffer(self, &domain[doffset + 1], cur_len - doffset - 1);
        BitString_storeUint(self, 0, 8);
        cur_len = doffset;
    }
    BitString_storeBuffer(self, domain, cur_len);
    BitString_storeUint(self, 0, 8);
}

void encode_text(BitString_t* self,
                 uint8_t* data,
                 size_t data_len,
                 CellRef_t* out_ref,
                 bool* out_has_ref) {
    uint8_t storeMax = (1023 - self->data_cursor) / 8;
    if (data_len > storeMax) {
        BitString_t inner;
        CellRef_t innerRef;
        bool innerHasRef;
        CellRef_t selfRef;
        BitString_init(&inner);
        encode_text(&inner, &data[storeMax], data_len - storeMax, &innerRef, &innerHasRef);
        hash_Cell(&inner, &innerRef, innerHasRef ? 1 : 0, &selfRef);
        BitString_storeBuffer(self, data, storeMax);
        *out_ref = selfRef;
        *out_has_ref = true;
    } else {
        BitString_storeBuffer(self, data, data_len);
        *out_has_ref = false;
    }
}

bool sign_data_deserialize_old(buffer_t* buf, sign_data_ctx_t* ctx) {
    SAFE(buffer_read_u32(buf, &ctx->schema_crc, BE));
    SAFE(buffer_read_u64(buf, &ctx->timestamp, BE));

    BitString_t bits;
    BitString_init(&bits);
    CellRef_t refs[4] = {0};
    int cur_ref = 0;

    switch (ctx->schema_crc) {
        case PLAINTEXT_REQUEST: {
            size_t len = buffer_remaining(buf);
            if (len > MAX_PLAINTEXT_LENGTH) {
                return false;
            }
            uint8_t* data;
            SAFE(buffer_read_ref(buf, &data, len));
            SAFE(check_ascii(data, len));
            add_hint_text(&ctx->hints, "Text", (char*) data, len);
            bool has_ref;
            encode_text(&bits, data, len, &refs[cur_ref], &has_ref);
            if (has_ref) {
                cur_ref++;
            }
            break;
        }
        case APP_DATA_REQUEST: {
            bool has_address;
            SAFE(buffer_read_bool(buf, &has_address));
            if (has_address) {
                address_t addr;
                SAFE(buffer_read_address(buf, &addr));
                add_hint_address(&ctx->hints, "Contract address", addr, true);
                BitString_storeBit(&bits, 1);
                BitString_storeAddress(&bits, addr.chain, addr.hash);
            } else {
                BitString_storeBit(&bits, 0);
            }

            bool has_domain;
            SAFE(buffer_read_bool(buf, &has_domain));
            if (!has_address && !has_domain) {
                return false;
            }
            if (has_domain) {
                uint8_t domain_len;
                SAFE(buffer_read_u8(buf, &domain_len));
                if (domain_len > MAX_APP_DATA_DOMAIN_LENGTH) {
                    return false;
                }
                uint8_t* domain;
                SAFE(buffer_read_ref(buf, &domain, domain_len));
                SAFE(check_ascii(domain, domain_len));
                BitString_t inner;
                BitString_init(&inner);
                encode_domain(&inner, domain, domain_len);
                hash_Cell(&inner, NULL, 0, &refs[cur_ref]);
                cur_ref++;
                BitString_storeBit(&bits, 1);
                add_hint_text(&ctx->hints, "App domain", (char*) domain, domain_len);
            } else {
                BitString_storeBit(&bits, 0);
            }

            SAFE(buffer_read_cell_ref(buf, &refs[cur_ref]));
            add_hint_hash(&ctx->hints, "Data hash", refs[cur_ref].hash);
            cur_ref++;

            bool has_ext;
            SAFE(buffer_read_bool(buf, &has_ext));
            if (has_ext) {
                SAFE(buffer_read_cell_ref(buf, &refs[cur_ref]));
                add_hint_hash(&ctx->hints, "Extension hash", refs[cur_ref].hash);
                cur_ref++;
                BitString_storeBit(&bits, 1);
            } else {
                BitString_storeBit(&bits, 0);
            }
            break;
        }
        default: {
            return false;
        }
    }

    CellRef_t out;
    SAFE(hash_Cell(&bits, refs, cur_ref, &out));
    memmove(ctx->cell_hash, out.hash, HASH_LEN);

    return true;
}

bool init_hash(cx_sha256_t* state) {
    return cx_sha256_init_no_throw(state) == CX_OK;
}

bool apply_hash(cx_hash_t* state, const uint8_t* data, size_t data_len) {
    return cx_hash_no_throw(state, 0, data, data_len, NULL, 0) == CX_OK;
}

bool apply_hash_uint64(cx_hash_t* state, uint64_t value, int size) {
    for (int i = size - 1; i >= 0; i--) {
        uint8_t part = (value >> (i * 8)) & 0xff;
        SAFE(apply_hash(state, &part, 1));
    }
    return true;
}

bool finalize_hash(cx_hash_t* state, uint8_t* hash, uint8_t hash_len) {
    return cx_hash_no_throw(state, CX_LAST, NULL, 0, hash, hash_len) == CX_OK;
}

bool sign_data_deserialize_new(buffer_t* buf, sign_data_ctx_t* ctx) {
    SAFE(buffer_read_u8(buf, &ctx->type_id));

    uint8_t address_flags;
    SAFE(buffer_read_u8(buf, &address_flags));

    ctx->display_testnet = (address_flags & P2_ADDR_FLAG_TESTNET) > 0;

    ctx->workchain = (address_flags & P2_ADDR_FLAG_MASTERCHAIN) > 0 ? -1 : 0;

    if ((address_flags & P2_ADDR_FLAG_WALLET_SPECIFIERS) > 0) {
        SAFE(buffer_read_bool(buf, &ctx->is_v3r2));
        SAFE(buffer_read_u32(buf, &ctx->subwallet_id, BE));
    } else {
        ctx->is_v3r2 = false;
        ctx->subwallet_id = DEFAULT_SUBWALLET_ID;
    }

    uint8_t app_domain_len;
    SAFE(buffer_read_u8(buf, &app_domain_len));
    if (app_domain_len > MAX_APP_DATA_DOMAIN_LENGTH) {
        return false;
    }
    uint8_t* app_domain;
    SAFE(buffer_read_ref(buf, &app_domain, app_domain_len));
    SAFE(check_ascii(app_domain, app_domain_len));

    SAFE(buffer_read_u64(buf, &ctx->timestamp, BE));

    if (!pubkey_to_hash(ctx->raw_public_key,
        ctx->subwallet_id,
        ctx->is_v3r2,
        ctx->address_hash,
        sizeof(ctx->address_hash))) {
        return false;
    }

    address_t addr;
    addr.chain = ctx->workchain == -1 ? 0xff : 0;
    memmove(addr.hash, ctx->address_hash, HASH_LEN);

    add_hint_address(&ctx->hints, "Wallet address", addr, false);

    add_hint_text(&ctx->hints, "App domain", (char*) app_domain, app_domain_len);

    switch (ctx->type_id) {
        case TEXT_TYPE_ID: {
            size_t len = buffer_remaining(buf);
            if (len > MAX_PLAINTEXT_LENGTH) {
                return false;
            }
            uint8_t* data;
            SAFE(buffer_read_ref(buf, &data, len));
            SAFE(check_ascii(data, len));
            add_hint_text(&ctx->hints, "Text", (char*) data, len);

            cx_sha256_t state;
            SAFE(init_hash(&state));
            cx_hash_t* st = (cx_hash_t*) &state;

            // sizeof - 1 because const strings are null terminated
            SAFE(apply_hash(st, TON_CONNECT_SIGN_DATA_STR, sizeof(TON_CONNECT_SIGN_DATA_STR) - 1));
            SAFE(apply_hash_uint64(st, (uint64_t) ctx->workchain, 4));
            SAFE(apply_hash(st, ctx->address_hash, sizeof(ctx->address_hash)));
            SAFE(apply_hash_uint64(st, (uint64_t) app_domain_len, 4));
            SAFE(apply_hash(st, app_domain, app_domain_len));
            SAFE(apply_hash_uint64(st, ctx->timestamp, 8));
            SAFE(apply_hash(st, SIGN_DATA_TXT_PREFIX, sizeof(SIGN_DATA_TXT_PREFIX) - 1));
            SAFE(apply_hash_uint64(st, (uint64_t) len, 4));
            SAFE(apply_hash(st, data, len));
            SAFE(finalize_hash(st, ctx->cell_hash, HASH_LEN));

            break;
        }
        case BINARY_TYPE_ID: {
            size_t len = buffer_remaining(buf);
            if (len > MAX_PLAINTEXT_LENGTH) {
                return false;
            }
            uint8_t* data;
            SAFE(buffer_read_ref(buf, &data, len));

            cx_sha256_t state;
            SAFE(init_hash(&state));
            cx_hash_t* st = (cx_hash_t*) &state;

            SAFE(apply_hash(st, data, len));
            SAFE(finalize_hash(st, ctx->data_hash, HASH_LEN));

            add_hint_hash(&ctx->hints, "Data hash", ctx->data_hash);

            SAFE(init_hash(&state));
            // sizeof - 1 because const strings are null terminated
            SAFE(apply_hash(st, TON_CONNECT_SIGN_DATA_STR, sizeof(TON_CONNECT_SIGN_DATA_STR) - 1));
            SAFE(apply_hash_uint64(st, (uint64_t) ctx->workchain, 4));
            SAFE(apply_hash(st, ctx->address_hash, sizeof(ctx->address_hash)));
            SAFE(apply_hash_uint64(st, (uint64_t) app_domain_len, 4));
            SAFE(apply_hash(st, app_domain, app_domain_len));
            SAFE(apply_hash_uint64(st, ctx->timestamp, 8));
            SAFE(apply_hash(st, SIGN_DATA_BIN_PREFIX, sizeof(SIGN_DATA_BIN_PREFIX) - 1));
            SAFE(apply_hash_uint64(st, (uint64_t) len, 4));
            SAFE(apply_hash(st, data, len));
            SAFE(finalize_hash(st, ctx->cell_hash, HASH_LEN));

            break;
        }
        case CELL_TYPE_ID: {
            uint32_t schema_crc;
            SAFE(buffer_read_u32(buf, &schema_crc, BE));

            add_hint_number(&ctx->hints, "Schema hash", schema_crc);

            uint8_t encoded_domain[MAX_APP_DATA_DOMAIN_LENGTH+1];
            int encoded_len = encode_domain_buf(app_domain, app_domain_len, encoded_domain, sizeof(encoded_domain));
            if (encoded_len < 0) {
                return false;
            }

            CellRef_t payload;
            SAFE(buffer_read_cell_ref(buf, &payload));

            BitString_t bits;
            CellRef_t refs[2] = {0};
            int cur_ref = 0;
            bool has_ref = false;

            BitString_init(&bits);
            BitString_storeUint(&bits, 0x75569022, 32);
            BitString_storeUint(&bits, schema_crc, 32);
            BitString_storeUint(&bits, ctx->timestamp, 64);
            BitString_storeAddress(&bits, addr.chain, addr.hash);

            BitString_t inner;
            CellRef_t innerRef;

            BitString_init(&inner);
            encode_text(&inner, encoded_domain, encoded_len, &innerRef, &has_ref);
            SAFE(hash_Cell(&inner, &innerRef, has_ref ? 1 : 0, &refs[cur_ref++]));

            refs[cur_ref++] = payload;

            CellRef_t out;
            SAFE(hash_Cell(&bits, refs, cur_ref, &out));
            memmove(ctx->cell_hash, out.hash, HASH_LEN);

            memmove(ctx->data_hash, payload.hash, HASH_LEN);
            add_hint_hash(&ctx->hints, "Data hash", ctx->data_hash);

            break;
        }
        default: {
            return false;
        }
    }

    return true;
}

bool sign_data_deserialize(buffer_t* buf, sign_data_ctx_t* ctx) {
    if (ctx->new_format) {
        return sign_data_deserialize_new(buf, ctx);
    } else {
        return sign_data_deserialize_old(buf, ctx);
    }
}
