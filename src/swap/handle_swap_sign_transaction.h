#pragma once

#include <stdbool.h>

#define MAX_SWAP_TOKEN_LENGTH 15

bool swap_check_validity();
void __attribute__((noreturn)) swap_finalize_exchange_sign_transaction(bool is_success);
