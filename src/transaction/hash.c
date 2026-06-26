#include <stdbool.h>
#include <string.h>  // memmove

#include "cx.h"

#include "hash.h"

#include "../common/cell.h"
#include "../common/bits.h"
#include "../constants.h"

bool serialize_message(message_t *msg, CellRef_t *ref) {
    BitString_t bits;

    BitString_init(&bits);
    BitString_storeBit(&bits, 0);                                    // tag
    BitString_storeBit(&bits, 1);                                    // ihr_disabled
    BitString_storeBit(&bits, msg->bounce ? 1 : 0);                  // bounce
    BitString_storeBit(&bits, 0);                                    // bounced
    BitString_storeAddressNull(&bits);                               // from
    BitString_storeAddress(&bits, msg->to.chain, msg->to.hash);      // to
    BitString_storeCoinsBuf(&bits, msg->value_buf, msg->value_len);  // amount
    BitString_storeBit(&bits, 0);       // Currency collection (not supported)
    BitString_storeCoins(&bits, 0);     // ihr_fees
    BitString_storeCoins(&bits, 0);     // fwd_fees
    BitString_storeUint(&bits, 0, 64);  // CreatedLT
    BitString_storeUint(&bits, 0, 32);  // CreatedAt

    // Refs
    if (msg->has_payload && msg->has_state_init) {
        BitString_storeBit(&bits, 1);  // state-init
        BitString_storeBit(&bits, 1);  // state-init ref
        BitString_storeBit(&bits, 1);  // body in ref

        CellRef_t internalMessageRefs[2];

        // Create refs
        internalMessageRefs[0] = msg->state_init;
        internalMessageRefs[1] = msg->payload;

        // Hash cell
        if (!hash_Cell(&bits, internalMessageRefs, 2, ref)) {
            return false;
        }
    } else if (msg->has_payload) {
        BitString_storeBit(&bits, 0);  // no state-init
        BitString_storeBit(&bits, 1);  // body in ref

        // Hash cell
        if (!hash_Cell(&bits, &msg->payload, 1, ref)) {
            return false;
        }
    } else if (msg->has_state_init) {
        BitString_storeBit(&bits, 1);  // no state-init
        BitString_storeBit(&bits, 1);  // state-init ref
        BitString_storeBit(&bits, 0);  // body inline

        // Hash cell
        if (!hash_Cell(&bits, &msg->state_init, 1, ref)) {
            return false;
        }
    } else {
        BitString_storeBit(&bits, 0);  // no state-init
        BitString_storeBit(&bits, 0);  // body inline

        // Hash cell
        if (!hash_Cell(&bits, NULL, 0, ref)) {
            return false;
        }
    }

    return true;
}

bool hash_tx(transaction_ctx_t *ctx) {
    BitString_t bits;

    if (ctx->message_count == 0 || ctx->message_count > MAX_MESSAGES) {
        return false;
    }

    CellRef_t internalMessageRefs[MAX_MESSAGES];

    for (int i = 0; i < ctx->message_count; i++) {
        if (!serialize_message(&ctx->messages[i], &internalMessageRefs[i])) {
            return false;
        }
    }

    //
    // Order
    //

    struct CellRef_t orderRef;
    BitString_init(&bits);
    BitString_storeUint(&bits, ctx->transaction.subwallet_id, 32);  // Wallet ID
    BitString_storeUint(&bits, ctx->transaction.timeout, 32);       // Timeout
    BitString_storeUint(&bits, ctx->transaction.seqno, 32);         // Seqno
    if (ctx->transaction.include_wallet_op) {
        BitString_storeUint(&bits, 0, 8);  // Simple order
    }
    for (int i = 0; i < ctx->message_count; i++) {
        BitString_storeUint(&bits, ctx->messages[i].send_mode, 8);
    }
    if (!hash_Cell(&bits, internalMessageRefs, ctx->message_count, &orderRef)) {
        return false;
    }

    // Result
    memmove(ctx->m_hash, orderRef.hash, HASH_LEN);

    return true;
}
