# HTTP Deserialize Implementation Review Plan

## Context

Reviewing HTTP/1.1 request deserializer. Several bugs/issues identified. Some fixed, some pending.

## Completed

- [x] Transparent robin_map lookup (string_hash/string_equal with is_transparent)
- [x] to_lowercase/is_lowercase in strings.hpp/strings.cpp
- [x] Tests use lowercase header keys
- [x] Case-insensitive headers + duplicate combining tests
- [x] Unit tests for to_lowercase/is_lowercase
- [x] DoS: Unbounded Content-Length - added max_body_size (default 1 MiB), returns 413

## Remaining Tasks

### 1. Fix trailing fields CRLF consumption

**Location:** `src/v1/deserialize/request.cpp:263`

**Problem:** `make_parse_stop` returns offset 0. Empty field line CRLF bytes not consumed. For pipelining, bytes leak into next request.

**Steps:**
1. Write failing test demonstrating CRLF leak
2. Fix: return `parse_result::more()` with `field_line_offset`

```cpp
[&](request_state_trailing_fields) {
    return parse_result<meta::result<request_state, status_type>>::more(
        request_state_complete{}, field_line_offset
    );
},
```

### 2. Fix Transfer-Encoding parsing edge case

**Location:** `src/v1/deserialize/request.cpp:317`

**Problem:** Splits on `", "` (comma-space). RFC allows `","` with optional whitespace. `"gzip,chunked"` fails.

**Steps:**
1. Write failing test with `gzip,chunked` (no space)
2. Fix: split on `","` then strip whitespace from each token

### 3. Verify remainder_buffer behavior

**Location:** `src/v1/deserialize/request.cpp:87`, `include/sl/http/v1/detail/machine.hpp:19-51`

**Question:** Does remainder_buffer reallocate unboundedly?

**Steps:**
1. Check if remainder bounded by input buffer size
2. If unbounded, add max size check

### 4. Consider pipelining test

**Location:** `src/v1/deserialize/request.cpp:129-131`

**Note:** Generator not fully consumed on success. Assert removed. Write test verifying pipelining works (unconsumed bytes available for next request).

### 5. ~~Pass config via const& for max_field_size~~

**Done:** Config now passed as `const request_config&`. Added `max_field_size` to config (default 80 KiB). Threaded through `parse_request`, `parse_request_part`, `parse_fields_finalize`.

## Current Limits

| Component | Limit |
|-----------|-------|
| Method | enum_max_str_length |
| Target | 8000 bytes |
| Version | enum_max_str_length |
| Headers total | 80 KiB |
| Chunk line | 8 bytes + 8 KiB |
| Content-Length body | max_body_size (1 MiB default) |
| Chunked body total | **NONE** |

## Test Status

- strings_test: 10/10 PASS
- deserialize_request_test: 24/24 PASS

## Files Modified

```
include/sl/http/v1/detail/strings.hpp
include/sl/http/v1/types/fields.hpp
include/sl/http/v1/deserialize/request.hpp
src/v1/detail/strings.cpp
src/v1/deserialize/request.cpp
test/src/v1_detail_strings_test.cpp
test/src/v1_detail_deserialize_request_test.cpp
```
