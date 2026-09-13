cmake_minimum_required(VERSION 3.22)
project(VirtualFileSystem
    VERSION 1.0.0
    LANGUAGES C CXX
    DESCRIPTION "A small virtual file system and asset packing tool."
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VFS_USE_SDL "Enabled SDL3 support for VFS" ON)

add_library(VirtualFileSystem STATIC
    src/vfs.cpp
    src/vfs_c.cpp
)
add_library(vfs::vfs ALIAS VirtualFileSystem)

target_include_directories(VirtualFileSystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(PROJECT_IS_TOP_LEVEL)
    if(MSVC)
        target_compile_options(VirtualFileSystem PRIVATE /W4 /permissive-)
    else()
        target_compile_options(VirtualFileSystem PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endif()

if(VFS_USE_SDL)
    if(NOT TARGET SDL3::SDL3)
        find_package(SDL3 CONFIG QUIET)
    endif()

    # Fallback only if developing standalone and SDL3 is not on system
    if(NOT TARGET SDL3::SDL3 AND PROJECT_IS_TOP_LEVEL)
        message(STATUS "[VFS] SDL3 not found. Fetching fallback SDL3 for standalone test...")
        include(FetchContent)
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        release-3.2.0
        )
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL3)
    endif()

    if(TARGET SDL3::SDL3)
        # PUBLIC because <vfs/VirtualFileSystem.hpp> includes <SDL3/SDL.h>
        target_link_libraries(VirtualFileSystem PUBLIC SDL3::SDL3)
        target_compile_definitions(VirtualFileSystem PUBLIC VFS_USE_SDL)
    else()
        message(WARNING "[VFS] SDL3 requested but not found. Building without SDL3 support.")
    endif()
endif()

if(NOT CMAKE_CROSSCOMPILING OR HOST_BUILD)
    add_executable(Packer 
        src/packer.cpp
    )
    add_executable(vfs::packer ALIAS Packer)

    target_include_directories(Packer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    set_target_properties(Packer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

if(PROJECT_IS_TOP_LEVEL)
    enable_testing()

    set(TEST_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets")

    add_executable(vfs_test tests/vfs_test.cpp)
    target_link_libraries(vfs_test PRIVATE vfs::vfs)

    add_custom_command(TARGET vfs_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${TEST_ASSETS_DIR}"
                "$<TARGET_FILE_DIR:vfs_test>/assets"
        COMMENT "Copying test assets next to vfs_test executable..."
    )

    add_test(
        NAME 1_TestPackerCLI
        COMMAND $<TARGET_FILE:Packer>
                "${TEST_ASSETS_DIR}"
                "$<TARGET_FILE_DIR:vfs_test>/test.dat"
                "1.0.0"
    )

    add_test(
        NAME 2_TestVFS_FolderMode
        COMMAND vfs_test folder
    )
    
    add_test(
        NAME 3_TestVFS_PackMode
        COMMAND vfs_test pack
    )
    # Ensure Test 3 only runs after the packer has successfully created test.dat
    set_tests_properties(3_TestVFS_PackMode PROPERTIES DEPENDS 1_TestPackerCLI)

endif()