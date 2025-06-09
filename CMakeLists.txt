    cmake_minimum_required(VERSION 3.31)
    project(cdo C)

    set(CMAKE_C_STANDARD 11)

    # For Homebrew on Apple Silicon, CMake might need this path hint.
    # You can also set this in CLion's CMake options: -DCMAKE_PREFIX_PATH=/opt/homebrew
    if(APPLE AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    endif()

    find_package(minizip REQUIRED)

    add_executable(cdo
            binary_io.c
            data_structures.c
            main.c
            utils.c
            zip_parser.c
            # Headers are generally not listed in add_executable
            # but can be useful for IDEs to display them.
            binary_io.h
            data_structures.h
            utils.h
            zip_parser.h
    )

    # Add project's own include directory (e.g., for "zip_parser.h")
    target_include_directories(cdo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Add minizip's include directories and link its libraries
    if(minizip_FOUND) # True if find_package(minizip REQUIRED) succeeded
        # Prefer using an imported target if minizip provides one (e.g., minizip::minizip)
        # This handles include directories and libraries automatically.
        # Check minizip's documentation or CMake's Findminizip.cmake for the exact target name.
        if(TARGET minizip::minizip)
            message(STATUS "Linking against imported target minizip::minizip")
            target_link_libraries(cdo PRIVATE minizip::minizip)
            # Fallback to using conventional variables if no imported target
        elseif(MINIZIP_INCLUDE_DIRS AND MINIZIP_LIBRARIES)
            message(STATUS "Using MINIZIP_INCLUDE_DIRS and MINIZIP_LIBRARIES")
            target_include_directories(cdo PRIVATE ${MINIZIP_INCLUDE_DIRS})
            target_link_libraries(cdo PRIVATE ${MINIZIP_LIBRARIES})
            # Some Find modules might use all lowercase
        elseif(minizip_INCLUDE_DIRS AND minizip_LIBRARIES)
            message(STATUS "Using minizip_INCLUDE_DIRS and minizip_LIBRARIES (lowercase)")
            target_include_directories(cdo PRIVATE ${minizip_INCLUDE_DIRS})
            target_link_libraries(cdo PRIVATE ${minizip_LIBRARIES})
        else()
            message(WARNING "minizip found, but its include/library CMake variables (e.g., MINIZIP_INCLUDE_DIRS, minizip::minizip) were not set as expected. Check CMake's Findminizip.cmake module or your minizip installation.")
        endif()
    endif()
    