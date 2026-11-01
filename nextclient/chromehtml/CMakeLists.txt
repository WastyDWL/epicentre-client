# chromehtml: ChromeHTML_Controller_001 on Chromium 150 (CEF), replacing Valve's CEF1-based chromehtml.dll.
# Also builds standalone (for its unit tests): cmake -S nextclient/chromehtml -B <dir> -DEPICENTRE_CEF_ROOT=<cef>
cmake_minimum_required(VERSION 3.21)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(chromehtml LANGUAGES C CXX)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    set(CHROMEHTML_STANDALONE ON)
endif ()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "chromehtml is 32-bit only, like the engine")
endif ()

# --- CEF binary distribution ---
set(CHROMEHTML_CEF_VERSION "150.0.23+g4ccc4ca+chromium-150.0.7871.287")
set(EPICENTRE_CEF_ROOT "" CACHE PATH "Extracted cef_binary_${CHROMEHTML_CEF_VERSION}_windows32_minimal (downloaded when empty)")
if (NOT EPICENTRE_CEF_ROOT)
    include(FetchContent)
    string(REPLACE "+" "%2B" _cef_file "cef_binary_${CHROMEHTML_CEF_VERSION}_windows32_minimal.tar.bz2")
    FetchContent_Declare(cef_binary
            URL "https://cef-builds.spotifycdn.com/${_cef_file}"
            URL_HASH SHA1=06b89aa0742c3efcb865afcfae08d9e7044650d4
            DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(cef_binary)
    set(EPICENTRE_CEF_ROOT "${cef_binary_SOURCE_DIR}")
endif ()
set(CEF_ROOT "${EPICENTRE_CEF_ROOT}")
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
# Windows SDK 10.0.19041 predates NTDDI_WIN10_FE that CEF 150 asks for.
list(REMOVE_ITEM CEF_COMPILER_DEFINES "NTDDI_VERSION=NTDDI_WIN10_FE")
list(APPEND CEF_COMPILER_DEFINES "NTDDI_VERSION=0x0A000008")
add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper)

set(CHROMEHTML_CORE_SOURCES
        src/common/log.cpp
        src/common/rtti.cpp
        src/core/api_registry.cpp
        src/core/api_router.cpp
        src/core/api_scheme.cpp
        src/core/browser.cpp
        src/core/cef_loader.cpp
        src/core/config.cpp
        src/core/controller.cpp
        src/core/domains.cpp
        src/core/engine.cpp
        src/core/frame_buffer.cpp
        src/core/game_bridge.cpp
        src/core/js_api.cpp
        src/core/keymap.cpp
        src/core/main_thread_queue.cpp
        src/core/menu_files.cpp
        src/core/menu_scheme.cpp
        src/core/navigation_gate.cpp
        src/core/vgui_panel.cpp
        src/core/view_math.cpp)

add_library(chromehtml_core STATIC ${CHROMEHTML_CORE_SOURCES})
target_include_directories(chromehtml_core PUBLIC src include ${CEF_INCLUDE_PATH})
target_compile_features(chromehtml_core PUBLIC cxx_std_20)
target_compile_definitions(chromehtml_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(chromehtml_core PUBLIC libcef_dll_wrapper ${CEF_LIB_RELEASE} delayimp)
SET_LIBRARY_TARGET_PROPERTIES(chromehtml_core)
# libcef.dll is delay-loaded from <game>\browser\cef\ (see cef_loader.cpp): an older CEF1 libcef.dll
# may still sit in the game root from a previous NextClient install.
target_link_options(chromehtml_core INTERFACE /DELAYLOAD:libcef.dll)

add_library(chromehtml SHARED src/exports.cpp)
target_link_libraries(chromehtml PRIVATE chromehtml_core)
SET_LIBRARY_TARGET_PROPERTIES(chromehtml)

add_executable(ncb_subprocess WIN32 subprocess/main.cpp src/core/js_api.cpp subprocess/ncb_subprocess.manifest)
target_include_directories(ncb_subprocess PRIVATE src ${CEF_INCLUDE_PATH})
target_compile_features(ncb_subprocess PRIVATE cxx_std_20)
target_link_libraries(ncb_subprocess PRIVATE libcef_dll_wrapper ${CEF_LIB_RELEASE} delayimp)
SET_EXECUTABLE_TARGET_PROPERTIES(ncb_subprocess)

# Runtime files installed to <game>\browser\cef\ next to ncb_subprocess.exe.
set(CHROMEHTML_CEF_RUNTIME_DIRS "${CEF_ROOT}/Release" "${CEF_ROOT}/Resources" PARENT_SCOPE)

if (CHROMEHTML_STANDALONE OR NEXTCLIENT_BUILD_TESTS)
    file(GLOB CHROMEHTML_TEST_SOURCES tests/*.cpp)
    add_executable(chromehtml-tests ${CHROMEHTML_TEST_SOURCES})
    target_link_libraries(chromehtml-tests PRIVATE chromehtml_core)
    SET_EXECUTABLE_TARGET_PROPERTIES(chromehtml-tests)
    # The fake vgui2 classes need RTTI like the game's client.dll (CEF's flags turn it off).
    set_source_files_properties(tests/test_vgui_panel.cpp PROPERTIES COMPILE_OPTIONS "/GR")
    if (NOT CHROMEHTML_STANDALONE)
        add_test(NAME chromehtml-tests COMMAND chromehtml-tests)
    endif ()
endif ()

if (CHROMEHTML_STANDALONE)
    # Loads chromehtml.dll through CreateInterface the way hw.dll does.
    add_executable(chromehtml_harness tools/harness.cpp)
    target_include_directories(chromehtml_harness PRIVATE src)
    target_compile_features(chromehtml_harness PRIVATE cxx_std_20)
endif ()
