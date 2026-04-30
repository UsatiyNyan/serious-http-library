# Target Deserialization Implementation Plan [COMPLETED]

## Overview

Replace placeholder `target_type = std::string` with structured variant parsing RFC 9112 request-target forms + query string key-value extraction.

## Type Design

### target_type (variant)

```cpp
using query_params = std::vector<std::pair<std::string, std::string>>;

struct target_origin_form {
    std::string path;       // decoded
    std::string raw_path;   // original for reconstruction
    query_params query;
    std::string raw_query;
};

struct target_absolute_form {
    std::string scheme;     // "http" or "https"
    std::string authority;  // host[:port]
    std::string path;
    std::string raw_path;
    query_params query;
    std::string raw_query;
};

struct target_authority_form {
    std::string host;
    std::uint16_t port;
};

struct target_asterisk_form {};

using target_type = std::variant<
    target_origin_form,
    target_absolute_form,
    target_authority_form,
    target_asterisk_form
>;
```

**Rationale:**
- Variant enforces valid state at compile-time
- Vector for query preserves order, supports duplicate keys
- Both raw and decoded strings for proxying + application use

## Parsing Algorithm

### Dispatch (by prefix)

```
"*"           → asterisk_form
"/"           → origin_form
"http[s]://"  → absolute_form
"host:port"   → authority_form (no "/" present)
else          → error
```

### Origin Form: `/path?query`

1. Split on `?` → path, query
2. Validate path characters
3. Percent-decode path
4. Parse query into pairs (split `&`, then `=`)
5. Decode keys/values (handle `+` as space)

### Authority Form: `host:port`

1. Split on last `:`
2. Parse port via `std::from_chars`
3. Validate port range (0-65535)

### Percent Decoding

- `%XX` → decode hex pair
- Query: also `+` → space
- Return null on invalid encoding

## Files to Modify

| File | Change |
|------|--------|
| `include/sl/http/v1/types/target.hpp` | Replace string alias with variant + form structs |
| `include/sl/http/v1/detail/target.hpp` | Full parsing implementation |
| `include/sl/http/v1/detail/percent_encoding.hpp` | New: percent decode utilities |
| `test/src/v1_detail_deserialize_target_test.cpp` | New: target parsing tests |
| `test/src/v1_detail_deserialize_request_test.cpp` | Update tests using target |
| `test/CMakeLists.txt` | Add new test target |

## Implementation Order

1. `types/target.hpp` - type definitions
2. `detail/percent_encoding.hpp` - decode utilities
3. `detail/target.hpp` - parsing logic
4. `v1_detail_deserialize_target_test.cpp` - unit tests
5. Update request tests for new target type
6. CMakeLists.txt

## Key Test Cases

**Origin form:**
- `/` (root)
- `/path/to/resource`
- `/search?q=hello&page=1`
- `/path%20with%20spaces` (percent decode)
- `/search?q=hello+world` (+ as space)
- `/path?flag&key=` (empty values)
- `/path?key=1&key=2` (duplicate keys)

**Absolute form:**
- `http://example.com/path?q=1`
- `https://host:8443/api`

**Authority form:**
- `example.com:443`
- `localhost:8080`

**Errors:**
- Empty string
- Invalid percent encoding (`%GG`, `%2`, `%`)
- Invalid port (`99999`, `abc`, empty host)
- Unrecognized form

## Verification

```bash
# Build
cmake --build build

# Run new tests
./build/test/sl_http_v1_detail_deserialize_target_test

# Run existing tests (ensure no regression)
./build/test/sl_http_v1_detail_deserialize_request_test
./build/test/sl_http_v1_detail_strings_test
```

---

## Completion Summary

**Date:** 2026-04-30

**Final test results:** 76/76 tests passing

**Files created:**
- `include/sl/http/v1/detail/percent_encoding.hpp`
- `src/v1/detail/percent_encoding.cpp`
- `src/v1/detail/target.cpp`
- `test/src/v1_detail_deserialize_target_test.cpp`

**Files modified:**
- `include/sl/http/v1/types/target.hpp` - variant + form structs
- `include/sl/http/v1/detail/target.hpp` - parse declarations
- `test/src/v1_detail_deserialize_request_test.cpp` - updated for new type
- `CMakeLists.txt` - added new sources
- `test/CMakeLists.txt` - added target test

**Notes:**
- Struct names use `_target_type` suffix (e.g., `origin_target_type`) per existing convention
- Namespace `sl::http::v1::deserialize::detail` for internal parsing functions
- `std::from_chars` for port parsing (no exceptions, handles overflow)
