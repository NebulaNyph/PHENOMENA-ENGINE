cmake_minimum_required(VERSION 3.31)

project(
    PHENOMENA
    VERSION 1.0.0
    DESCRIPTION "PHENOMENA 3D Engine"
    LANGUAGES C CXX
)

list(
    APPEND
    CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_LIST_DIR}/cmake"
    "${CMAKE_CURRENT_LIST_DIR}/cmake/description"
    "${CMAKE_CURRENT_LIST_DIR}/cmake/code_quality"
)

if(CMAKE_CROSSCOMPILING)
    list(
        APPEND
        CMAKE_FIND_ROOT_PATH
        "${CMAKE_CURRENT_LIST_DIR}/cmake"
        "${CMAKE_CURRENT_LIST_DIR}/cmake/description"
        "${CMAKE_CURRENT_LIST_DIR}/cmake/code_quality"
    )
endif()

find_package(cpm CONFIG REQUIRED)

CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.1
)

CPMAddPackage(
    NAME tinygltf
    GITHUB_REPOSITORY syoyo/tinygltf
    GIT_TAG v2.9.7
)

find_package(SDL3 CONFIG REQUIRED)

find_package(warnings CONFIG REQUIRED)

if(PROJECT_IS_TOP_LEVEL)
    find_package(clang_format CONFIG REQUIRED)
    find_package(clang_tidy CONFIG REQUIRED)
    find_package(clang_doc CONFIG REQUIRED)
    find_package(cpplint CONFIG REQUIRED)
    find_package(ct_doxygen CONFIG REQUIRED)
endif()

add_subdirectory(src)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)

    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()

    include(GNUInstallDirs)

    find_package(package_description CONFIG REQUIRED)

    string(
        APPEND
        CPACK_SYSTEM_NAME
        "-${CMAKE_SYSTEM_PROCESSOR}"
    )

    include(CPack)
endif()
