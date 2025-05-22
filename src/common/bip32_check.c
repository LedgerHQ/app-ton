#include <stdbool.h>

#include "../globals.h"

#include "bip32_check.h"

bool check_bip32_path_len(uint8_t bip32_path_len) {
    return ((bip32_path_len <= 2) ? false : true);
}

bool check_global_bip32_path() {
    return check_bip32_path_len(G_context.bip32_path_len);
}
