# CMake packages
# provides fmt, assert
cpmaddpackage(
        NAME serious-meta-library
        GIT_REPOSITORY "git@github.com:UsatiyNyan/serious-meta-library.git"
        GIT_TAG 1.0.0)
cpmaddpackage(
        NAME serious-execution-library
        GIT_REPOSITORY "git@github.com:UsatiyNyan/serious-execution-library.git"
        GIT_TAG 1.0.1)


sl_target_link_system_libraries(${PROJECT_NAME}
        PUBLIC
        # serious libraries
        sl::meta
        sl::exec
)
