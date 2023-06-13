# TON commands

## Overview

| Command name | INS | Description |
| --- | --- | --- |
| `GET_VERSION` | 0x03 | Get application version as `MAJOR`, `MINOR`, `PATCH` |
| `GET_APP_NAME` | 0x04 | Get ASCII encoded application name |
| `GET_PUBLIC_KEY` | 0x05 | Get public key given BIP32 path |
| `SIGN_TX` | 0x06 | Sign transaction given BIP32 path and raw transaction |
| `GET_ADDRESS_PROOF` | 0x08 | Sign an address proof in TON Connect 2 compatible format given BIP32 path and proof parameters |

## GET_VERSION

### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x03 | 0x00 | 0x00 | 0x00 | - |

### Response

| Response length (bytes) | SW | RData |
| --- | --- | --- |
| 3 | 0x9000 | `MAJOR (1)` \|\| `MINOR (1)` \|\| `PATCH (1)` |

## GET_APP_NAME

### Command

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x04 | 0x00 | 0x00 | 0x00 | - |

### Response

| Response length (bytes) | SW | RData |
| --- | --- | --- |
| var | 0x9000 | `APPNAME (var)` |

## GET_PUBLIC_KEY

### Command

Use P2 to control what kind of address to present to user:
* set bit 0x01 to make address testnet only
* set bit 0x02 to use masterchain instead of basechain for the address

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x05 | 0x00 (no display) <br> 0x01 (display) | 0x00-0x03 | 1 + 4n | `len(bip32_path) (1)` \|\|<br> `bip32_path{1} (4)` \|\|<br>`...` \|\|<br>`bip32_path{n} (4)` |

### Response

| Response length (bytes) | SW | RData |
| --- | --- | --- |
| 32 | 0x9000 |  `public_key (32)` |

## SIGN_TX

### Command

Sent as series of packages. First one contains bip32 path:

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x06 | 0x00 | 0x03 (first & more) | 1 + 4n | `len(bip32_path) (1)` \|\|<br> `bip32_path{1} (4)` \|\|<br>`...` \|\|<br>`bip32_path{n} (4)` |

Then an arbitrary number of chunks with transaction data, up to a total of 510 bytes (currently, max valid transaction data length is 299 bytes).

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x06 | 0x00 | 0x02 (more) <br> 0x00 (last) | `len(chunk)` | `chunk` |

### Response

| Response length (bytes) | SW | RData |
| --- | --- | --- |
| 98 | 0x9000 | `len(signature) (1)` \|\| <br> `signature (64)` \|\| <br> `len(hash) (1)` \|\| <br> `hash (32)` \|\||

## GET_ADDRESS_PROOF

### Command

Use P2 to control what kind of address to present to user:
* set bit 0x01 to make address testnet only
* set bit 0x02 to use masterchain instead of basechain for the address

Payload's length is implicitly calculated from buffer length.

Proofs are generated according to this [spec](https://github.com/ton-blockchain/ton-connect/blob/main/requests-responses.md#address-proof-signature-ton_proof).

| CLA | INS | P1 | P2 | Lc | CData |
| --- | --- | --- | --- | --- | --- |
| 0xE0 | 0x08 | 0x01 | 0x00-0x03 | 1 + 4n + 1 + d + 8 + p | `len(bip32_path) (1)` \|\|<br> `bip32_path{1} (4)` \|\|<br>`...` \|\|<br>`bip32_path{n} (4)` \|\|<br> `len(app_domain) == d (1)` \|\|<br> `app_domain (d)` \|\|<br> `timestamp (8)` \|\|<br> `payload (p)` |

### Response

| Response length (bytes) | SW | RData |
| --- | --- | --- |
| 98 | 0x9000 | `len(signature) (1)` \|\| <br> `signature (64)` \|\| <br> `len(hash) (1)` \|\| <br> `hash (32)` \|\||

## Status Words

| SW | SW name | Description |
| --- | --- | --- |
| 0x6985 | `SW_DENY` | Rejected by user |
| 0x6A86 | `SW_WRONG_P1P2` | Either `P1` or `P2` is incorrect |
| 0x6A87 | `SW_WRONG_DATA_LENGTH` | `Lc` or minimum APDU length is incorrect |
| 0x6D00 | `SW_INS_NOT_SUPPORTED` | No command exists with `INS` |
| 0x6E00 | `SW_CLA_NOT_SUPPORTED` | Bad `CLA` used for this application |
| 0xB000 | `SW_WRONG_RESPONSE_LENGTH` | Wrong response length (buffer size problem) |
| 0xB002 | `SW_DISPLAY_ADDRESS_FAIL` | Address conversion to string failed |
| 0xB003 | `SW_DISPLAY_AMOUNT_FAIL` | Amount conversion to string failed |
| 0xB004 | `SW_WRONG_TX_LENGTH` | Wrong raw transaction length |
| 0xB005 | `SW_TX_PARSING_FAIL` | Failed to parse raw transaction |
| 0xB007 | `SW_BAD_STATE` | Security issue with bad state |
| 0xB008 | `SW_SIGNATURE_FAIL` | Signature of raw transaction failed |
| 0xB00B | `SW_REQUEST_TOO_LONG` | The request is too long |
| 0x9000 | `OK` | Success |
