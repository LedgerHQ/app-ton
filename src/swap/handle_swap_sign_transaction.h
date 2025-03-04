#pragma once

#include <stdbool.h>

bool swap_check_validity();
void __attribute__((noreturn)) swap_finalize_exchange_sign_transaction(bool is_success);
