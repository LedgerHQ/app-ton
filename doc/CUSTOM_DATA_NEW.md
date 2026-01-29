# List of supported types of custom data

Taken from the [spec](https://github.com/ton-blockchain/ton-connect/blob/main/requests-responses.md#sign-data).

| Type ID (Ledger App specific) | Name | Description |
| --- | --- | --- |
| `0x00` | Text | Human-readable text |
| `0x01` | Binary | Binary data |
| `0x02` | Cell | Data serialized as a cell |

# The general serialization format

| Field | Size (bytes) or type | Description |
| --- | :---: | --- |
| `type_id` | 1 | Type ID |
| `address_flags` | 1 | 0x01 - testnet, 0x02 - masterchain, 0x04 - include is_v3r2 flag and subwallet_id |
| `is_v3r2` | 0 or 1 (if `address_flags & 0x04`) | Use v3r2 address
| `subwallet_id` | 0 or 4 (if `address_flags & 0x04`) | Set a non-default subwallet ID (big endian) |
| `app_domain_len` | 1 | Length of the app domain |
| `app_domain` | `app_domain_len` | ASCII-only app domain (not the zero-terminated reversed format!) |
| `timestamp` | 8 | Timestamp to be used for signing |
| `request` | `var` | The serialized request data according to the schema (see below) |

## Text (0x00)

Payload prefix is `txt` according to the spec. The message will be displayed to the user as is, and used as the payload.

Ledger request format:
| Value | Length or type | Description |
| --- | --- | --- |
| `message` | 0-120 | ASCII-only message |

## Binary (0x01)

Payload prefix is `bin` according to the spec. The hash of the data will be displayed to the user, and used as the payload.

Ledger request format:
| Value | Length or type | Description |
| --- | --- | --- |
| `data` | `var` | Binary data |

## Cell (0x02)

TL-B schema:
```tlb
message#75569022 schema_hash:uint32 timestamp:uint64 userAddress:MsgAddress 
					{n:#} appDomain:^(SnakeData ~n) payload:^Cell = Message;
```
(`appDomain` is encoded in zero-terminated reversed format)

Ledger request format:
| Value | Length or type | Description |
| --- | --- | --- |
| `schema_crc` | 4 | Big-endian schema CRC |
| `payload` | `cell_ref` | The cell |
