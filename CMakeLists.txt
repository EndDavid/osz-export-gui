cmake_minimum_required(VERSION 3.14)
project(osz_export_gui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EUI_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(EUI_BUILD_USER_APPS OFF CACHE BOOL "" FORCE)
set(EUI_BUILD_TEST_FIXTURES OFF CACHE BOOL "" FORCE)
set(EUI_ENABLE_TRAY OFF CACHE BOOL "" FORCE)
set(EUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(EUI_ENABLE_MODULES OFF CACHE BOOL "" FORCE)

set(EUI_NEO_SOURCE "E:/EUItest/EUI-NEO" CACHE PATH "EUI-NEO source checkout")
add_subdirectory("${EUI_NEO_SOURCE}" "${CMAKE_BINARY_DIR}/eui-neo")

# Use the app's own icon for the Windows executable resource (overrides EUI-NEO's default).
if(WIN32)
    set(APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/osz_app_icon.rc")
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico" APP_ICON_ICO_NATIVE)
    string(REPLACE "\\" "\\\\" APP_ICON_ICO_ESCAPED "${APP_ICON_ICO_NATIVE}")
    file(WRITE "${APP_ICON_RC}" "IDI_APP_ICON ICON \"${APP_ICON_ICO_ESCAPED}\"\n")
    set_property(GLOBAL PROPERTY EUI_NEO_APP_ICON_RESOURCE "${APP_ICON_RC}")
endif()

add_executable(osz_export_gui WIN32 main.cpp)
eui_neo_configure_app(osz_export_gui)

if(WIN32)
    target_link_libraries(osz_export_gui PRIVATE shell32)
endif()

# Deploy the app icon over EUI-NEO's default so the window/taskbar icon matches.
add_custom_command(TARGET osz_export_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
        "$<TARGET_FILE_DIR:osz_export_gui>/assets/icon.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico"
        "$<TARGET_FILE_DIR:osz_export_gui>/assets/icon.ico"
    COMMENT "Deploying application icon")

# Managed helper (.NET) + Realm native library, deployed next to the executable.
set(OSZ_HELPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/helper" CACHE PATH "Directory containing OszExporter.dll and the Realm native library")
add_custom_command(TARGET osz_export_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${OSZ_HELPER_DIR}"
        "$<TARGET_FILE_DIR:osz_export_gui>/helper"
    COMMENT "Deploying osz export helper")
