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

# Path to the runtime lib (DLL on Windows), exposed for the copy helper below.
set(TCXNATNET_RUNTIME_LIB "${NATNET_RUNTIME}" CACHE INTERNAL "NatNet runtime lib to copy beside the exe")

# tcxnatnet_copy_runtime(<exe-target>)
#   Call this from your app's CMakeLists AFTER trussc_app(). On Windows it adds a
#   post-build step that copies NatNetLib.dll next to the executable, so the app
#   runs without manually copying the DLL. No-op on Linux/macOS (rpath handles it).
function(tcxnatnet_copy_runtime target)
    if(WIN32 AND TCXNATNET_RUNTIME_LIB)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TCXNATNET_RUNTIME_LIB}" "$<TARGET_FILE_DIR:${target}>"
            COMMENT "[tcxNatNet] copying NatNetLib.dll next to ${target}")
    endif()
endfunction()
