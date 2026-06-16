cmake_minimum_required(VERSION 3.16)
set(ADDON_NAME tcxNatNet)

# -----------------------------------------------------------------------------
# Locate the user-provided OptiTrack NatNet SDK (NOT bundled — see README).
# Search order: -DNATNET_SDK_ROOT=...  then  libs/natnet/
# Expected layout under the root:
#   include/NatNetClient.h , NatNetTypes.h , ...
#   lib/x64/NatNetLib.lib + NatNetLib.dll        (Windows)
#   lib/libNatNet.so                             (Linux)
# -----------------------------------------------------------------------------
if(NOT NATNET_SDK_ROOT)
    set(NATNET_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libs/natnet")
endif()

set(_natnet_inc "${NATNET_SDK_ROOT}/include")
find_path(NATNET_INCLUDE_DIR NatNetClient.h HINTS "${_natnet_inc}" "${NATNET_SDK_ROOT}")

if(WIN32)
    find_library(NATNET_IMPLIB NAMES NatNetLib HINTS "${NATNET_SDK_ROOT}/lib/x64" "${NATNET_SDK_ROOT}/lib")
    find_file(NATNET_RUNTIME NatNetLib.dll HINTS "${NATNET_SDK_ROOT}/lib/x64" "${NATNET_SDK_ROOT}/lib")
else()
    find_library(NATNET_RUNTIME NAMES NatNet HINTS "${NATNET_SDK_ROOT}/lib" "${NATNET_SDK_ROOT}/lib/x64")
endif()

if(NOT NATNET_INCLUDE_DIR OR (WIN32 AND NOT NATNET_IMPLIB) OR (NOT WIN32 AND NOT NATNET_RUNTIME))
    message(FATAL_ERROR
        "\n[tcxNatNet] OptiTrack NatNet SDK not found.\n"
        "  This addon does not ship the SDK (OptiTrack EULA).\n"
        "  1. Download it from https://optitrack.com/support/downloads/developer-tools.html\n"
        "  2. Place its 'include/' and 'lib/' under:  ${NATNET_SDK_ROOT}\n"
        "     (or reconfigure with -DNATNET_SDK_ROOT=/path/to/NatNetSDK)\n"
        "  Searched include: ${_natnet_inc}\n")
endif()

# Imported target. Linking the SHARED imported lib makes CMake add its directory
# to the build-tree rpath, so libNatNet.so resolves at runtime on Linux.
add_library(natnet SHARED IMPORTED)
set_target_properties(natnet PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${NATNET_INCLUDE_DIR}")
if(WIN32)
    set_target_properties(natnet PROPERTIES
        IMPORTED_IMPLIB "${NATNET_IMPLIB}"
        IMPORTED_LOCATION "${NATNET_RUNTIME}")
else()
    set_target_properties(natnet PROPERTIES
        IMPORTED_LOCATION "${NATNET_RUNTIME}")
endif()

set(CMAKE_CXX_STANDARD 20)          # TrussC core headers require C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB ADDON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB ADDON_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_library(${ADDON_NAME} STATIC ${ADDON_SOURCES} ${ADDON_HEADERS})
target_include_directories(${ADDON_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${ADDON_NAME} PUBLIC TrussC natnet)

add_library(tc::${ADDON_NAME} ALIAS ${ADDON_NAME})

set(TCXNATNET_RUNTIME_LIB "${NATNET_RUNTIME}" CACHE INTERNAL "NatNet runtime lib")

# Windows: NatNetLib.dll must sit next to the .exe at runtime. We can't rely on
# the app/example CMakeLists for this (trusscli regenerates those), so the copy
# lives here in the addon CMakeLists. We defer to the end of the app's top-level
# scope — by then trussc_app() has created the executable — and attach a
# post-build copy to every executable target. No-op on Linux/macOS (the build
# embeds an rpath to libNatNet.so). Requires CMake >= 3.19 for cmake_language(DEFER);
# on older CMake, copy the DLL manually (see README).
if(WIN32 AND NATNET_RUNTIME AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
    function(_tcxNatNet_copyDllToExecutables)
        get_property(_tgts DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_t IN LISTS _tgts)
            get_target_property(_type ${_t} TYPE)
            if(_type STREQUAL "EXECUTABLE")
                add_custom_command(TARGET ${_t} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${TCXNATNET_RUNTIME_LIB}" "$<TARGET_FILE_DIR:${_t}>"
                    COMMENT "[tcxNatNet] copying NatNetLib.dll next to ${_t}")
            endif()
        endforeach()
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _tcxNatNet_copyDllToExecutables)
endif()
