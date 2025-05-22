#pragma once

#include <stdbool.h>  // bool

#include "../types.h"  // MAX_BIP32_PATH

/**
 * Check if the given BIP32 path length is valid.
 *
 * @param bip32_path_len The length of the BIP32 path.
 *
 * @return true if the BIP32 path len is valid, false otherwise.
 *
 */
bool check_bip32_path_len(uint8_t bip32_path_len);

/**
 * Check the bip32 path len stored in G_context.
 *
 * @return true if the BIP32 path len is valid, false otherwise.
 *
 */
bool check_global_bip32_path();
