#ifdef HAVE_SWAP

#include "handle_swap_sign_transaction.h"
#include "display.h"
#include "swap.h"
#include "string.h"
#include "os_lib.h"
#include "constants.h"
#include "os_utils.h"
#include "globals.h"
#include "sw.h"
#include "os.h"
#include "display_transaction.h"
#include "base64.h"
#include "format_address.h"
#include "transaction_hints.h"

typedef struct swap_validated_s {
    bool initialized;
    uint8_t decimals;
    char ticker[MAX_SWAP_TOKEN_LENGTH];
    uint8_t amount_length;
    uint8_t amount[MAX_VALUE_BYTES_LEN];
    char recipient[G_ADDRESS_LEN];
} swap_validated_t;

static swap_validated_t G_swap_validated;

// Save the BSS address where we will write the return value when finished
static uint8_t* G_swap_sign_return_value_address;

// Save the data validated during the Exchange app flow
bool swap_copy_transaction_parameters(create_transaction_parameters_t* params) {
    PRINTF("Inside Ton swap_copy_transaction_parameters\n");

    // Ensure no extraid
    if (params->destination_address_extra_id == NULL) {
        PRINTF("destination_address_extra_id expected\n");
        return false;
    } else if (params->destination_address_extra_id[0] != '\0') {
        PRINTF("destination_address_extra_id expected empty, not '%s'\n",
               params->destination_address_extra_id);
        return false;
    }

    if (params->destination_address == NULL) {
        PRINTF("Destination address expected\n");
        return false;
    }

    if (params->amount == NULL) {
        PRINTF("Amount expected\n");
        return false;
    }

    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with app globals
    // and also because we want to memset the whole bss segment as it is not done
    // when an app is called as a lib.
    // This is necessary as many part of the code expect bss variables to
    // initialized at 0.
    swap_validated_t swap_validated;
    memset(&swap_validated, 0, sizeof(swap_validated));

    // Parse config and save decimals and ticker
    // If there is no coin_configuration, consider that we are doing a TRX swap
    if (params->coin_configuration == NULL) {
        memcpy(swap_validated.ticker, "TON", sizeof("TON"));
        swap_validated.decimals = EXPONENT_SMALLEST_UNIT;
    } else {
        if (!swap_parse_config(params->coin_configuration,
                               params->coin_configuration_length,
                               swap_validated.ticker,
                               sizeof(swap_validated.ticker),
                               &swap_validated.decimals)) {
            PRINTF("Fail to parse coin_configuration\n");
            return false;
        }
    }

    // Save recipient
    strlcpy(swap_validated.recipient,
            params->destination_address,
            sizeof(swap_validated.recipient));
    if (swap_validated.recipient[sizeof(swap_validated.recipient) - 1] != '\0') {
        PRINTF("Address copy error\n");
        return false;
    }

    // Save amount
    if (params->amount_length > sizeof(swap_validated.amount)) {
        PRINTF("Amount too big\n");
        return false;
    } else {
        swap_validated.amount_length = params->amount_length;
        memcpy(swap_validated.amount, params->amount, params->amount_length);
    }

    swap_validated.initialized = true;

    // Full reset the global variables
    os_explicit_zero_BSS_segment();

    // Keep the address at which we'll reply the signing status
    G_swap_sign_return_value_address = &params->result;

    // Commit from stack to global data, params becomes tainted but we won't access it anymore
    memcpy(&G_swap_validated, &swap_validated, sizeof(swap_validated));
    return true;
}

bool swap_check_validity() {
    PRINTF("Inside Ton swap_check_validity\n");

    if (!G_swap_validated.initialized) {
        return false;
    }

    // char operation[G_OPERATION_LEN];
    // char amount[G_AMOUNT_LEN];
    // char address[G_ADDRESS_LEN];
    // char payload[G_PAYLOAD_LEN];
    // char address_title[G_ADDRESS_TITLE_LEN];
    // if (!display_transaction(operation,
    //                          sizeof(operation),
    //                          amount,
    //                          sizeof(amount),
    //                          address,
    //                          sizeof(address),
    //                          payload,
    //                          sizeof(payload),
    //                          address_title,
    //                          sizeof(address_title))) {
    //     PRINTF("Failed to format transaction fields\n");
    //     return io_send_sw(SW_SWAP_FAILURE);
    // }

    if (G_context.tx_info.transaction.is_blind) {
        PRINTF("Blind operation not allowed in swap mode\n");
        return io_send_sw(SW_SWAP_FAILURE);
    }

    if (G_context.tx_info.transaction.hints_type != TRANSACTION_COMMENT) {
        PRINTF("Wrong operation %d\n", G_context.tx_info.transaction.hints_type);
        return io_send_sw(SW_SWAP_FAILURE);
    } else if (G_context.tx_info.transaction.hints_len != 0) {
        PRINTF("Hint length %d refused\n", G_context.tx_info.transaction.hints_len);
        return io_send_sw(SW_SWAP_FAILURE);
    } else {
        PRINTF("Valid operation %d\n", G_context.tx_info.transaction.hints_type);
    }

    if (G_context.tx_info.transaction.send_mode & 128) {
        PRINTF("Amount MAX is refused\n");
        return io_send_sw(SW_SWAP_FAILURE);
    }

    if (G_swap_validated.amount_length != G_context.tx_info.transaction.value_len)  {
        PRINTF("Amount length does not match, promised %d, received %d\n",
               G_swap_validated.amount_length,
               G_context.tx_info.transaction.value_len);
        return io_send_sw(SW_SWAP_FAILURE);
    } else if (memcmp(G_swap_validated.amount, G_context.tx_info.transaction.value_buf, G_swap_validated.amount_length) != 0) {
        PRINTF("Amount does not match, promised %.*H, received %.*H\n",
               G_swap_validated.amount_length,
               G_swap_validated.amount,
               G_swap_validated.amount_length,
               G_context.tx_info.transaction.value_buf);
        return io_send_sw(SW_SWAP_FAILURE);
    } else {
        PRINTF("Amounts match %.*H\n", G_context.tx_info.transaction.value_len, G_context.tx_info.transaction.value_buf);
    }

    char encoded_address[G_ADDRESS_LEN];
    uint8_t decoded_address[ADDRESS_LEN] = {0};
    if (!address_to_friendly(G_context.tx_info.transaction.to.chain,
                             G_context.tx_info.transaction.to.hash,
                             G_context.tx_info.transaction.bounce,
                             false,
                             decoded_address,
                             sizeof(decoded_address))) {
        PRINTF("!address_to_friendly\n");
        return io_send_sw(SW_SWAP_FAILURE);
    }
    memset(encoded_address, 0, sizeof(encoded_address));
    base64_encode(decoded_address, sizeof(decoded_address), encoded_address, sizeof(encoded_address));

    if (strcmp(G_swap_validated.recipient, encoded_address) != 0) {
        PRINTF("Destination does not match, promised %s, received %s\n",
               G_swap_validated.recipient,
               encoded_address);
        return io_send_sw(SW_SWAP_FAILURE);
    } else {
        PRINTF("Destination %s is valid\n", encoded_address);
    }

    return true;
}

void __attribute__((noreturn)) swap_finalize_exchange_sign_transaction(bool is_success) {
    *G_swap_sign_return_value_address = is_success;
    os_lib_end();
}

#endif  // HAVE_SWAP
