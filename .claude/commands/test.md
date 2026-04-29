# Test

Build and run tests.

## Usage

- `/test` - build and run all tests
- `/test request` - build and run request deserializer tests
- `/test strings` - build and run strings tests
- `/test machine` - build and run machine tests
- `/test <filter>` - run with gtest filter (e.g., `/test *TransferEncoding*`)

## Targets

| Short | Full target |
|-------|-------------|
| all | `serious-http-library_tests` |
| request | `serious-http-library_v1_detail_deserialize_request_test` |
| strings | `serious-http-library_v1_detail_strings_test` |
| machine | `serious-http-library_v1_detail_deserialize_machine_test` |

## Instructions

1. Parse argument: `$ARGUMENTS`
2. If empty or "all": build `serious-http-library_tests`, run `ctest --test-dir build`
3. If matches short name (request/strings/machine): build that target, run executable
4. Otherwise: treat as gtest filter, build all, run with `--gtest_filter=$ARGUMENTS`

Build command: `cmake --build build --target <target> -j`

Test executables in: `./build/test/`
