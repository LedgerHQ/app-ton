/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include "deserialize.h"
#include "types.h"
#include "common/mybuffer.h"
#include "../common/cell.h"
#include "transaction_hints.h"
#include "../constants.h"
#include "../common/types.h"
#ifndef FUZZ
#include "../crypto.h"
#include "../globals.h"
#endif

#define SAFE(RES, CODE) \
    if (!RES) {         \
        return CODE;    \
    }

parser_status_e transaction_deserialize(buffer_t *buf, transaction_t *tx) {
    if (buf->size > MAX_TRANSACTION_LEN) {
        return WRONG_LENGTH_ERROR;
    }

    // tag
    SAFE(buffer_read_u8(buf, &tx->tag), TAG_PARSING_ERROR);
    if (tx->tag >= 0x02) {  // Only 0x00 and 0x01 are supported now
        return TAG_PARSING_ERROR;
    }

    if (tx->tag == 0x01) {
        SAFE(buffer_read_u32(buf, &tx->subwallet_id, BE), GENERAL_ERROR);

        uint8_t flags;
        SAFE(buffer_read_u8(buf, &flags), GENERAL_ERROR);

        tx->include_wallet_op = (flags & 0x01) != 0;

        if (flags & 0x04) {
            uint8_t expected_public_key[PUBKEY_LEN];
            SAFE(buffer_read_buffer(buf, expected_public_key, PUBKEY_LEN), GENERAL_ERROR);

#ifndef FUZZ
            uint8_t actual_public_key[PUBKEY_LEN];
            if (crypto_derive_public_key(G_context.bip32_path,
                                         G_context.bip32_path_len,
                                         actual_public_key) < 0) {
                return GENERAL_ERROR;
            }

            if (memcmp(expected_public_key, actual_public_key, PUBKEY_LEN) != 0) {
                return PUBLIC_KEY_MISMATCH_ERROR;
            }
#endif
        }
    } else {
        tx->subwallet_id = DEFAULT_SUBWALLET_ID;
        tx->include_wallet_op = true;
    }

    // Basic Transaction parameters
    SAFE(buffer_read_u32(buf, &tx->seqno, BE), SEQ_PARSING_ERROR);
    SAFE(buffer_read_u32(buf, &tx->timeout, BE), TIMEOUT_PARSING_ERROR);

    return PARSING_OK;
}

parser_status_e message_deserialize(buffer_t *buf, transaction_t *full_tx, message_t *tx) {
    if (buf->size > MAX_TRANSACTION_LEN) {
        return WRONG_LENGTH_ERROR;
    }

    full_tx->hints.hints_count = 0;

    SAFE(buffer_read_varuint(buf, &tx->value_len, tx->value_buf, MAX_VALUE_BYTES_LEN),
         VALUE_PARSING_ERROR);
    SAFE(buffer_read_address(buf, &tx->to), TO_PARSING_ERROR);
    SAFE(buffer_read_bool(buf, &tx->bounce), BOUNCE_PARSING_ERROR);
    SAFE(buffer_read_u8(buf, &tx->send_mode), SEND_MODE_PARSING_ERROR);

    // state-init
    SAFE(buffer_read_bool(buf, &tx->has_state_init), STATE_INIT_PARSING_ERROR);
    if (tx->has_state_init) {
        SAFE(buffer_read_cell_ref(buf, &tx->state_init), STATE_INIT_PARSING_ERROR);
    }

    // Payload
    SAFE(buffer_read_bool(buf, &tx->has_payload), PAYLOAD_PARSING_ERROR);
    if (tx->has_payload) {
        SAFE(buffer_read_cell_ref(buf, &tx->payload), STATE_INIT_PARSING_ERROR);
    }

    // Hints
    SAFE(buffer_read_bool(buf, &full_tx->has_hints), HINTS_PARSING_ERROR);
    if (full_tx->has_hints) {
        if (!tx->has_payload) {
            return HINTS_PARSING_ERROR;
        }
        SAFE(buffer_read_u32(buf, &full_tx->hints_type, BE), HINTS_PARSING_ERROR);
        SAFE(buffer_read_u16(buf, &full_tx->hints_len, BE), HINTS_PARSING_ERROR);
        SAFE(buffer_read_ref(buf, &full_tx->hints_data, full_tx->hints_len), HINTS_PARSING_ERROR);
    }

    // Process hints
    SAFE(process_hints(full_tx, tx), HINTS_PARSING_ERROR);

    if (full_tx->subwallet_id != DEFAULT_SUBWALLET_ID) {
        add_hint_number(&full_tx->hints, "Subwallet ID", (uint64_t) full_tx->subwallet_id);
    }

    return (buf->offset == buf->size) ? PARSING_OK : WRONG_LENGTH_ERROR;
}
