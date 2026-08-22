cmake_minimum_required(VERSION 3.24)

option(FULL_RELEASE "Finalizes the build, compressing everything into a BSA." OFF)

project(
    ThrowableWeaponsSKSE
    VERSION 1.0.0
    LANGUAGES CXX
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ENABLE_SKYRIM_SE OFF CACHE BOOL "Disable Skyrim SE support in CommonLibSSE-NG" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Enable Skyrim AE support in CommonLibSSE-NG" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "Disable Skyrim VR support in CommonLibSSE-NG" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE-NG tests" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "Enable CommonLibSSE-NG Xbyak support" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY OFF CACHE BOOL "Preserve existing trampoline behavior" FORCE)

include(FetchContent)
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG e7863a71523a2896c92ea9d3105c0d121dcdba0d
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(CommonLibSSE)

set(COMMONLIB_IDDB_HEADER "${commonlibsse_SOURCE_DIR}/include/REL/IDDB.h")
if(NOT EXISTS "${COMMONLIB_IDDB_HEADER}")
    message(FATAL_ERROR "CommonLibSSE-NG 6.4.0 format-5 support is missing. Delete the build directory and configure again.")
endif()
file(READ "${COMMONLIB_IDDB_HEADER}" COMMONLIB_IDDB_CONTENT)
if(NOT COMMONLIB_IDDB_CONTENT MATCHES "SSEv5" OR NOT COMMONLIB_IDDB_CONTENT MATCHES "load_v5")
    message(FATAL_ERROR "CommonLibSSE-NG source does not contain AE 1.7.99 Address Library format-5 support. Delete the build directory and configure again.")
endif()
unset(COMMONLIB_IDDB_CONTENT)

file(
    GLOB_RECURSE PLUGIN_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Common/PCH.h"
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
            $<$<CONFIG:Release>:/Zi>
            $<$<CONFIG:Release>:/FS>
    )

    target_link_options(
        ${PROJECT_NAME}
        PRIVATE
            $<$<CONFIG:Release>:/DEBUG:FULL>
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
    )

    set_target_properties(
        ${PROJECT_NAME}
        PROPERTIES
            PDB_NAME "${PROJECT_NAME}"
            PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/Release"
    )
endif()

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        CommonLibSSE::CommonLibSSE
)

find_package(jsoncpp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE JsonCpp::JsonCpp)

set(DATA_DIR "${PROJECT_SOURCE_DIR}/Data")

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    if(FULL_RELEASE)
        set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
    else()
        set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME} - Test")
    endif()
endif()

if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
            "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(EXISTS "${PROJECT_SOURCE_DIR}/Data/SKSE/Plugins/${PROJECT_NAME}")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E remove_directory
                "${OUTPUT_FOLDER}/SKSE/Plugins/${PROJECT_NAME}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${OUTPUT_FOLDER}/SKSE/Plugins/${PROJECT_NAME}"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/Data/SKSE/Plugins/${PROJECT_NAME}"
                "${OUTPUT_FOLDER}/SKSE/Plugins/${PROJECT_NAME}"
            VERBATIM
        )
    endif()

    if(EXISTS "${PROJECT_SOURCE_DIR}/Data/SKSE/Plugins/${PROJECT_NAME}.ini")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${PROJECT_SOURCE_DIR}/Data/SKSE/Plugins/${PROJECT_NAME}.ini"
                "${OUTPUT_FOLDER}/SKSE/Plugins/${PROJECT_NAME}.ini"
            VERBATIM
        )
    endif()
endif()
