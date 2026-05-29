cpmaddpackage(
        NAME serious-execution-library
        GIT_REPOSITORY "git@github.com:UsatiyNyan/serious-execution-library.git"
        GIT_TAG 3.0.1
        GIT_SHALLOW TRUE)
cpmaddpackage(
        NAME serious-io-library
        GIT_REPOSITORY "git@github.com:UsatiyNyan/serious-io-library.git"
        GIT_TAG 2.0.0
        GIT_SHALLOW TRUE)

sl_target_link_system_libraries(${PROJECT_NAME}
        INTERFACE
        # serious libraries
        sl::io
        sl::exec
)

sl_add_example(${PROJECT_NAME} 00_http_v1_server)
# sl_add_example(${PROJECT_NAME} 01_http_client)
