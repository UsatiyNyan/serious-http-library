server := "./build/examples/serious-http-library_01_http_v1_server_parallel"

# Run server and benchmark in parallel
bench:
    {{server}} 8080 128 $(nproc) &
    sleep 1
    ab -n 100000 -c 128 http://localhost:8080/
    pkill -f "{{server}}" || true
