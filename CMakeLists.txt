cmake_minimum_required(VERSION 3.21)

# Version : la GitHub Action la remplace par celle du tag (ex. v0.2.0 -> 0.2.0).
set(TOOLBOX_VERSION "0.4.0" CACHE STRING "Version de ToolBox")

# Dépendances C++ via vcpkg (Tesseract). Le fichier vcpkg.json liste les bibliothèques.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/triplets")
set(VCPKG_TARGET_TRIPLET "x64-windows-static-release" CACHE STRING "")
set(VCPKG_HOST_TRIPLET "x64-windows-static-release" CACHE STRING "")

project(ToolBox VERSION ${TOOLBOX_VERSION} LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "ToolBox ne se compile que sous Windows (MSVC).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Tesseract CONFIG REQUIRED)

set(DEPS_DIR ${CMAKE_BINARY_DIR}/deps)

# --- WebView2 SDK (NuGet) -------------------------------------------------
set(WEBVIEW2_VERSION "1.0.4258.31")
set(WEBVIEW2_DIR ${DEPS_DIR}/webview2-${WEBVIEW2_VERSION})
if(NOT EXISTS ${WEBVIEW2_DIR}/build/native/include/WebView2.h)
  file(DOWNLOAD
    https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${WEBVIEW2_VERSION}/microsoft.web.webview2.${WEBVIEW2_VERSION}.nupkg
    ${DEPS_DIR}/webview2.zip TLS_VERIFY ON STATUS dl_status)
  list(GET dl_status 0 dl_code)
  if(NOT dl_code EQUAL 0)
    message(FATAL_ERROR "Téléchargement WebView2 échoué : ${dl_status}")
  endif()
  file(ARCHIVE_EXTRACT INPUT ${DEPS_DIR}/webview2.zip DESTINATION ${WEBVIEW2_DIR})
endif()

# --- nlohmann/json (un seul en-tête) -------------------------------------
set(JSON_HPP ${DEPS_DIR}/nlohmann/json.hpp)
if(NOT EXISTS ${JSON_HPP})
  file(DOWNLOAD
    https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    ${JSON_HPP} TLS_VERIFY ON STATUS dl_status)
  list(GET dl_status 0 dl_code)
  if(NOT dl_code EQUAL 0)
    message(FATAL_ERROR "Téléchargement json.hpp échoué : ${dl_status}")
  endif()
endif()

# --- Interface : ui/*.html/css/js fusionnés en un seul fichier ------------
set(UI_BUNDLE ${CMAKE_BINARY_DIR}/ui_bundle.html)
file(GLOB UI_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/ui/*)
add_custom_command(
  OUTPUT ${UI_BUNDLE}
  COMMAND ${CMAKE_COMMAND} -DUI_DIR=${CMAKE_SOURCE_DIR}/ui -DOUT=${UI_BUNDLE}
          -P ${CMAKE_SOURCE_DIR}/cmake/bundle_ui.cmake
  DEPENDS ${UI_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/bundle_ui.cmake
  COMMENT "Assemblage de l'interface")
add_custom_target(ui_bundle DEPENDS ${UI_BUNDLE})

configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)
configure_file(res/toolbox.rc.in ${CMAKE_BINARY_DIR}/generated/toolbox.rc @ONLY)

add_executable(ToolBox WIN32
  src/main.cpp
  src/app.cpp
  src/settings.cpp
  src/updater.cpp
  src/util.cpp
  src/color_pick.cpp
  src/geo.cpp
  src/hotkey.cpp
  src/http.cpp
  src/screen_select.cpp
  src/modules/app_launcher.cpp
  src/modules/clipboard_history.cpp
  src/modules/color_picker.cpp
  src/modules/enter_guard.cpp
  src/modules/place_launcher.cpp
  src/modules/process_manager.cpp
  src/modules/screen_ocr.cpp
  src/modules/stats.cpp
  src/modules/system_monitor.cpp
  ${CMAKE_BINARY_DIR}/generated/toolbox.rc
)
add_dependencies(ToolBox ui_bundle)
set_source_files_properties(${CMAKE_BINARY_DIR}/generated/toolbox.rc
  PROPERTIES OBJECT_DEPENDS "${UI_BUNDLE};${CMAKE_SOURCE_DIR}/res/toolbox.ico")

target_include_directories(ToolBox PRIVATE
  src
  ${CMAKE_BINARY_DIR}/generated
  ${DEPS_DIR}
  ${WEBVIEW2_DIR}/build/native/include)

target_compile_definitions(ToolBox PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_compile_options(ToolBox PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /utf-8 /permissive- /EHsc>)

target_link_libraries(ToolBox PRIVATE
  ${WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib
  winhttp bcrypt dwmapi shell32 shlwapi advapi32 user32 ole32 comctl32 version
  wlanapi iphlpapi windowsapp secur32 msimg32 windowscodecs
  Tesseract::libtesseract)

set_target_properties(ToolBox PROPERTIES
  LINK_FLAGS "/MANIFESTUAC:\"level='asInvoker' uiAccess='false'\"")
