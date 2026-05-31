# serious-http-library
For serious programmers.

# About

## URI (target)

[Serialization](./include/sl/http/v1/serialize/target.hpp) and [deserialization](./include/sl/http/v1/deserialize/target.hpp) of URI according to RFC3986.

## v1

[Serialization](./include/sl/http/v1/serialize/message.hpp) and [deserialization](./include/sl/http/v1/deserialize/message.hpp) of HTTP/1.1 accoring to [RFC9112](https://www.rfc-editor.org/rfc/rfc9112.html).

## v2

🚧 TODO 🚧

Serialization, deserialization and session of HTTP/2 accoring to [RFC9113](https://www.rfc-editor.org/rfc/rfc9113.html).

# Bench

## 00_http_v1_server

Deps:
- serious-meta-library=2.1.0
- serious-execution-library=3.1.0
- serious-io-library=2.0.0

Hardware:
- Kernel: 6.12.87 
- CPU: AMD Ryzen 5 7600
- Memory: 32 GiB DDR5 5200 MT/s

Notes:
- monolithic executor - perf would be better on distributed executor

Command:
```sh
just bench
```

Apache Benchmark:
```
Concurrency Level:      256
Time taken for tests:   4.023 seconds
Complete requests:      100000
Failed requests:        0
Total transferred:      9800000 bytes
HTML transferred:       1400000 bytes
Requests per second:    24854.04 [#/sec] (mean)
Time per request:       10.300 [ms] (mean)
Time per request:       0.040 [ms] (mean, across all concurrent requests)
Transfer rate:          2378.61 [Kbytes/sec] received
```

# TODO

- [x] v1: [RFC9112](https://www.rfc-editor.org/rfc/rfc9112.html)
    - [ ] validation
    - [ ] token validation
    - [ ] arbitrary tokens
    - [ ] "visited bytes"
- [x] URI: [RFC3986](https://www.rfc-editor.org/rfc/rfc3986.html)
- [ ] v2: [RFC9113](https://www.rfc-editor.org/rfc/rfc9113.html)
- [ ] Websockets: [RFC6455](https://www.rfc-editor.org/rfc/rfc6455.html)
