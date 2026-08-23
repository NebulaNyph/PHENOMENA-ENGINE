add_executable(phenomena
    main.cpp
)

target_compile_features(phenomena
    PRIVATE
        cxx_std_20
)

target_link_libraries(phenomena
    PRIVATE
        SDL3::SDL3
        glm::glm
)

target_include_directories(phenomena
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(ANDROID)
    target_link_libraries(phenomena
        PRIVATE
            GLESv3
            android
            log
    )
else()
    find_package(OpenGL REQUIRED)

    target_link_libraries(phenomena
        PRIVATE
            OpenGL::GL
    )
endif()
