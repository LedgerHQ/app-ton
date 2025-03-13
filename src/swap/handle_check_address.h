#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

#define BASE_CHAIN             0x00
#define MASTER_CHAIN           0xFF
#define ADDRESS_BASE64_LENGTH  48
#define ADDRESS_DECODED_LENGTH 36

bool swap_decode_address(const char *address, uint8_t *decoded, size_t size);
