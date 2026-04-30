# HTTP Deserialize Implementation Review Plan

## Context

Reviewing HTTP/1.1 request deserializer. Several bugs/issues identified. Some fixed, some pending.

## Architecture

### Strengths

- **Variant-based state machine** (`request.hpp:73-82`) - type-safe transitions, compile-time verification
- **Zero-copy optimization** (`request.cpp:92-109`) - direct buffer parsing when remainder empty
- **Coroutine-based streaming** - `exec::async_gen` yields chunks as parsed, no full-body buffering
- **Result monad pattern** - `meta::result<T, E>` with `.map()`, `.and_then()` chains
- **Robin map with transparent lookup** - O(1) header access without allocation on lookup

### Design Decisions

- Field names lowercased on insert for case-insensitive matching
- Multi-value headers comma-concatenated per RFC 7230
- Two-phase body processing: headers finalize determines body type, then type-specific parsing
- Chunk streaming: yields each chunk immediately via async generator

## Completed

- [x] Transparent robin_map lookup (string_hash/string_equal with is_transparent)
- [x] to_lowercase/is_lowercase in strings.hpp/strings.cpp
- [x] Tests use lowercase header keys
- [x] Case-insensitive headers + duplicate combining tests
- [x] Unit tests for to_lowercase/is_lowercase
- [x] DoS: Unbounded Content-Length - added max_body_size (default 1 MiB), returns 413
- [x] Trailing fields CRLF consumption - use `make_parse_more` with `field_line_offset`
- [x] Transfer-Encoding parsing edge case - split on `,` + strip whitespace
- [x] Pipelining tests (commit 127b30e)

## Remaining Tasks

### 1. Verify remainder_buffer behavior

**Location:** `src/v1/deserialize/request.cpp:87`, `include/sl/http/v1/detail/machine.hpp:19-51`

**Question:** Does remainder_buffer reallocate unboundedly?

**Steps:**
1. Check if remainder bounded by input buffer size
2. If unbounded, add max size check

### 2. Chunked body total size limit

**Location:** `src/v1/deserialize/request.cpp:393-484`

**Issue:** Individual chunks limited but total chunked body has no limit. Could DoS with infinite small chunks.

**Fix:** Track cumulative size, enforce max_body_size.

### 3. Trailer field validation

**Location:** `src/v1/deserialize/request.cpp:273`

**Issue:** Trailers parsed same as headers. RFC 9110 forbids certain fields in trailers (Content-Length, Transfer-Encoding, Host, etc.).

**Fix:** Validate trailer field names against forbidden list.

### ~~4. Target parsing incomplete~~ DONE

Implemented structured parsing into variant of forms (origin, absolute, authority, asterisk) with query string key-value extraction. See `target-deserialization-plan.md` for design details.

### 5. Centralize magic numbers

**Locations:**
- 8000 bytes target limit (`request.cpp:197`)
- 8 KiB chunk extension limit (`request.cpp:399`)

**Fix:** Add to `request_config` struct like body/field limits.

## Current Limits

| Component | Limit | Configurable |
|-----------|-------|--------------|
| Method | enum_max_str_length | No |
| Target | 8000 bytes | No (hardcoded) |
| Version | enum_max_str_length | No |
| Headers total | 80 KiB | Yes (max_field_size) |
| Chunk line | 8 bytes + 8 KiB | No (hardcoded) |
| Content-Length body | 1 MiB | Yes (max_body_size) |
| Chunked body total | **NONE** | N/A |
| Trailer fields | Same as headers | Yes |

## Key Files

| File | Purpose |
|------|---------|
| `include/sl/http/v1/deserialize/request.hpp` | Public API, state machine types |
| `src/v1/deserialize/request.cpp` | Core parsing logic |
| `include/sl/http/v1/detail/machine.hpp` | Buffer management, parse results |
| `src/v1/detail/strings.cpp` | String utilities, delimiter finding |

## Test Status

- strings_test: 10/10 PASS
- deserialize_request_test: 28/28 PASS
