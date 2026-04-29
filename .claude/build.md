# Build Documentation

## Configure

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

## Targets

| Target | Description |
|--------|-------------|
| `serious-http-library` | Main library (sl::http alias) |
| `serious-http-library_tests` | All tests combined |
| `serious-http-library_v1_detail_strings_test` | Unit tests for strings utilities |
| `serious-http-library_v1_detail_deserialize_machine_test` | Unit tests for parse machine |
| `serious-http-library_v1_detail_deserialize_request_test` | Unit tests for request deserializer |

## Build

```bash
# Build all tests
cmake --build build --target serious-http-library_tests -j

# Build specific target
cmake --build build --target serious-http-library_v1_detail_deserialize_request_test -j
```

## Run Tests

```bash
# All tests
ctest --test-dir build

# Specific test
./build/test/serious-http-library_v1_detail_deserialize_request_test
```
