# CMake packages
# provides fmt, assert
cpmaddpackage(
        NAME serious-meta-library
        GIT_REPOSITORY "git@github.com:UsatiyNyan/serious-meta-library.git"
        GIT_TAG 2.0.0
        GIT_SHALLOW TRUE)

cpmaddpackage(
        NAME serious-execution-library
        GIT_REPOSITORY "git@github.com:UsatiyNyan/serious-execution-library.git"
        GIT_TAG 3.0.1
        GIT_SHALLOW TRUE)

sl_target_link_system_libraries(${PROJECT_NAME}
        PUBLIC
        # serious libraries
        sl::meta
        sl::exec
)
