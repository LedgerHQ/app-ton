#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display_transaction.h"

#include "../../transaction/types.h"
#include "../../common/format_address.h"
#include "../../common/format_bigint.h"
#include "../../globals.h"
#include "io.h"
#include "../../sw.h"
#include "../../common/base64.h"
#include "../../constants.h"

bool display_transaction(char *g_operation,
                         size_t g_operation_len,
                         char *g_amount,
                         size_t g_amount_len,
                         char *g_address,
                         size_t g_address_len,
                         char *g_payload,
                         size_t g_payload_len,
                         char *g_address_title,
                         size_t g_address_title_len) {
    // Operation
    memset(g_operation, 0, g_operation_len);
    snprintf(g_operation, g_operation_len, "%s", G_context.tx_info.transaction.title);

    message_t *msg = &G_context.tx_info.messages[G_context.tx_info.message_count - 1];

    // Amount
    memset(g_amount, 0, g_amount_len);
    if ((msg->send_mode & 128) != 0) {
        snprintf(g_amount, g_amount_len, "ALL YOUR TONs");
    } else {
        if (!amountToString(msg->value_buf,
                            msg->value_len,
                            EXPONENT_SMALLEST_UNIT,
                            "TON",
                            g_amount,
                            g_amount_len)) {
            io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
            return false;
        }
    }

    // Address
    uint8_t address[ADDRESS_LEN] = {0};
    if (!address_to_friendly(msg->to.chain,
                             msg->to.hash,
                             msg->bounce,
                             false,
                             address,
                             sizeof(address))) {
        io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
        return false;
    }
    memset(g_address, 0, g_address_len);
    base64_encode(address, sizeof(address), g_address, g_address_len);

    // Payload
    memset(g_payload, 0, g_payload_len);
    if (msg->has_payload) {
        base64_encode(msg->payload.hash,
                      HASH_LEN,
                      g_payload,
                      g_payload_len);
    } else {
        snprintf(g_payload, g_payload_len, "Nothing");
    }

    size_t recipient_len = strnlen(G_context.tx_info.transaction.recipient,
                                   sizeof(G_context.tx_info.transaction.recipient));
    if (recipient_len + 1 > g_address_title_len) {
        memmove(g_address_title, G_context.tx_info.transaction.recipient, g_address_title_len - 1);
        g_address_title[g_address_title_len - 1] = '\0';
    } else {
        memmove(g_address_title, G_context.tx_info.transaction.recipient, recipient_len + 1);
    }

    return true;
}