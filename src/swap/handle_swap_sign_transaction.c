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
#include "handle_check_address.h"
#include "../jetton.h"

// Error codes for swap, to be moved in SDK
#define ERROR_INTERNAL                0x00
#define ERROR_WRONG_AMOUNT            0x01
#define ERROR_WRONG_DESTINATION       0x02
#define ERROR_WRONG_FEES              0x03
#define ERROR_WRONG_METHOD            0x04
#define ERROR_CROSSCHAIN_WRONG_MODE   0x05
#define ERROR_CROSSCHAIN_WRONG_METHOD 0x06
#define ERROR_GENERIC                 0xFF

typedef struct swap_validated_s {
    bool initialized;
    uint8_t decimals;
    char ticker[MAX_TICKER_LEN];
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

    // Save recipient address in BASE64
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

static address_t* swap_get_tx_recipient_address(bool is_jetton_swap) {
    address_t* recipient = NULL;

    /*
     *  XXX:
     *   In case of Jetton transfer, transaction `to` address is the sender jetton wallet address.
     *   Recipient address is the second hints in message hints.
     */
    if (is_jetton_swap) {
        if (G_context.tx_info.transaction.hints.hints_count >= 2) {
            PRINTF("Tx recipient is a jetton wallet address\n");
            recipient = &G_context.tx_info.transaction.hints.hints[1].address.address;
        }
    } else {
        recipient = &G_context.tx_info.transaction.to;
    }

    /*
     * XXX:
     *  Shall not happened as the transaction type is check earlier, thus, recipient address
     *  is either message `to` or message hint[1] (in case of Jetton transfer).
     */
    if (recipient == NULL) {
        PRINTF("Missing tx recipient address\n");
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    PRINTF("Tx recipient raw address %.*H\n", HASH_LEN, recipient->hash);

    return recipient;
}

static void swap_get_new_owner_address(address_t* address) {
    uint8_t decoded[ADDRESS_DECODED_LENGTH];

    PRINTF("Swap recipient address %s\n", G_swap_validated.recipient);
    if (!swap_decode_address(G_swap_validated.recipient, decoded)) {
        PRINTF("Failed to decode recipient address\n");
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    memcpy(address->hash, &decoded[2], HASH_LEN);
    PRINTF("Swap recipient raw address %.*H\n", HASH_LEN, address->hash);
}

/**
 * @brief Compare recipient address received from swap to transaction recipient address
 *
 * From swap/exchange, base64 encoded owner address is received (even for Jetton swap)
 * From ton transaction:
 *  - Recipient is `to` address (i.e. new owner) in raw binary for native coin transfer.
 *  - Recipient is hint[1] and this is the jetton wallet address (new owner) in raw binary.
 */
static bool swap_compare_recipient_address(bool is_jetton_swap) {
    bool match = false;
    address_t* tx_recipient = swap_get_tx_recipient_address(is_jetton_swap);
    address_t swap_recipient;

    swap_get_new_owner_address(&swap_recipient);
    match = (memcmp(tx_recipient->hash, swap_recipient.hash, HASH_LEN) == 0);

    return match;
}

bool swap_check_validity(void) {
    PRINTF("Inside Ton swap_check_validity\n");

    if (!G_swap_validated.initialized) {
        PRINTF("Swap structure is not initialized\n");
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    if (G_context.tx_info.transaction.is_blind) {
        PRINTF("Blind operation not allowed in swap mode\n");
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    if ((G_context.tx_info.transaction.hints_type == TRANSACTION_COMMENT) &&
        (G_context.tx_info.transaction.hints_len != 0)) {
        PRINTF("Hint length %d refused\n", G_context.tx_info.transaction.hints_len);
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    } else if ((G_context.tx_info.transaction.hints_type != TRANSACTION_COMMENT) &&
               (G_context.tx_info.transaction.hints_type != TRANSACTION_TRANSFER_JETTON)) {
        PRINTF("Wrong operation %d\n", G_context.tx_info.transaction.hints_type);
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    bool is_jetton_swap =
        strncmp(G_swap_validated.ticker, "TON", sizeof("TON")) == 0 ? false : true;

    if (!is_jetton_swap &&
        (G_context.tx_info.transaction.hints_type == TRANSACTION_TRANSFER_JETTON)) {
        PRINTF("Wrong operation type %d for native coin swap\n",
               G_context.tx_info.transaction.hints_type);
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    if (is_jetton_swap && (G_context.tx_info.transaction.hints_type == TRANSACTION_COMMENT)) {
        PRINTF("Wrong operation type %d for Jetton swap\n",
               G_context.tx_info.transaction.hints_type);
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    PRINTF("Valid operation %d\n", G_context.tx_info.transaction.hints_type);

    if (G_context.tx_info.transaction.send_mode & 128) {
        PRINTF("Amount MAX is refused\n");
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    }

    uint8_t amount_length = 0;
    uint8_t* amount = NULL;
    // Depending on swap operation, we need to get the proper amount from the transaction
    if (is_jetton_swap) {
        amount_length = G_context.tx_info.transaction.hints.hints[0].amount.value_len;
        amount = G_context.tx_info.transaction.hints.hints[0].amount.value;
    } else {
        amount_length = G_context.tx_info.transaction.value_len;
        amount = G_context.tx_info.transaction.value_buf;
    }

    if (G_swap_validated.amount_length != amount_length) {
        PRINTF("Amount length does not match, promised %d, received %d\n",
               G_swap_validated.amount_length,
               amount_length);
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    } else if (memcmp(G_swap_validated.amount, amount, G_swap_validated.amount_length) != 0) {
        PRINTF("Amount does not match, promised %.*H, received %.*H\n",
               G_swap_validated.amount_length,
               G_swap_validated.amount,
               G_swap_validated.amount_length,
               amount);
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Amounts match %.*H\n", amount_length, amount);
    }

    if (!swap_compare_recipient_address(is_jetton_swap)) {
        PRINTF("Destination does not match\n");
        io_send_sw(SW_SWAP_FAILURE);
        // unreachable
        os_sched_exit(0);
    } else {
        PRINTF("Recipient addresses match\n");
    }

    return true;
}

void __attribute__((noreturn)) swap_finalize_exchange_sign_transaction(bool is_success) {
    PRINTF("Returning to Exchange with status %d\n", is_success);
    *G_swap_sign_return_value_address = is_success;
    os_lib_end();
}

#endif  // HAVE_SWAP
