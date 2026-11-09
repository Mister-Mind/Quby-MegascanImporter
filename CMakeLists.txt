cmake_minimum_required(VERSION 3.24)
project(QubyMegascanImporter VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Widgets Test Network)
qt_standard_project_setup()
enable_testing()
add_executable(policy_tests tests/policy_tests.cpp src/Policy.cpp)
target_include_directories(policy_tests PRIVATE src)
target_link_libraries(policy_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME policy COMMAND policy_tests)
add_executable(fab_preview_smoke tests/fab_preview_smoke.cpp src/FabPreview.cpp)
target_include_directories(fab_preview_smoke PRIVATE src)
target_link_libraries(fab_preview_smoke PRIVATE Qt6::Gui Qt6::Network)

set(WEBVIEW2_SDK "" CACHE PATH "Unpacked Microsoft.Web.WebView2 NuGet SDK")
set(WEBVIEW2_NATIVE "${WEBVIEW2_SDK}/build/native")
if(NOT EXISTS "${WEBVIEW2_NATIVE}/include/WebView2.h")
    message(FATAL_ERROR "Set WEBVIEW2_SDK to the unpacked Microsoft.Web.WebView2 package.")
endif()
qt_add_executable(QubyMegascanImporter WIN32 src/main.cpp src/Browser.cpp src/Browser.h src/Policy.cpp
    src/AssetLibrary.cpp src/AssetLibrary.h src/FabPreview.cpp)
add_custom_command(TARGET QubyMegascanImporter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:QubyMegascanImporter>/blender"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/blender/export_asset.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/blender/preview.py"
        "$<TARGET_FILE_DIR:QubyMegascanImporter>/blender")
target_compile_definitions(QubyMegascanImporter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(QubyMegascanImporter PRIVATE "${WEBVIEW2_NATIVE}/include")
target_link_libraries(QubyMegascanImporter PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Network
    "${WEBVIEW2_NATIVE}/x64/WebView2LoaderStatic.lib" ole32 version)
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(assets/icon.png PROPERTIES QT_RESOURCE_ALIAS icon.png)
set_target_properties(QubyMegascanImporter PROPERTIES OUTPUT_NAME "Quby - MegascanImporter")
qt_add_resources(QubyMegascanImporter quby PREFIX /quby FILES qml/Main.qml assets/icon.png)
