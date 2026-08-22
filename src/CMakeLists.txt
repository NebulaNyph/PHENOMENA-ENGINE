include(GNUInstallDirs)

add_executable(cli_app main.cpp)

target_compile_features(cli_app PRIVATE cxx_std_23)

target_link_libraries(
    cli_app
    PRIVATE warnings
)

install(
    TARGETS cli_app
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT runtime
)

if(EMSCRIPTEN)
    add_subdirectory(emscripten)
endif()

if(NOT TARGET SDL3::SDL3-shared)
    return()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_library(
        sdl_app
        SHARED
        sdl_main.cpp
        phenomena/runtime.cpp
    )
else()
    add_executable(
        sdl_app
        sdl_main.cpp
        phenomena/runtime.cpp
    )
endif()

target_compile_features(
    sdl_app
    PRIVATE
    cxx_std_23
)

target_link_libraries(
    sdl_app
    PRIVATE
    SDL3::SDL3-shared
    Microsoft.GSL::GSL
    warnings
)

target_include_directories(
    sdl_app
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(TARGET Tracy::TracyClient)
    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
        add_library(
            tracy_sandbox
            SHARED
            tracy_sandbox.cpp
        )
    else()
        add_executable(
            tracy_sandbox
            tracy_sandbox.cpp
        )
    endif()

    target_compile_features(
        tracy_sandbox
        PRIVATE
        cxx_std_23
    )

    target_link_libraries(
        tracy_sandbox
        PRIVATE
        SDL3::SDL3-shared
        Tracy::TracyClient
        Microsoft.GSL::GSL
        warnings
    )
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Android|Emscripten")
    return()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin|iOS")
    set_target_properties(
        sdl_app
        PROPERTIES
        INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}"
        MACOSX_RPATH ON
    )
else()
    set_target_properties(
        sdl_app
        PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
        BUILD_RPATH_USE_ORIGIN ON
    )
endif()

install(
    TARGETS sdl_app
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT runtime
)

if(
    CMAKE_VERSION VERSION_GREATER_EQUAL "3.21"
    AND CMAKE_SYSTEM_NAME STREQUAL "Windows"
)
    add_custom_command(
        TARGET sdl_app
        POST_BUILD

        COMMAND
            ${CMAKE_COMMAND}
            -E copy
            -t $<TARGET_FILE_DIR:sdl_app>
            $<TARGET_RUNTIME_DLLS:sdl_app>

        COMMAND_EXPAND_LISTS
    )
endif()

add_subdirectory(unity_build)
