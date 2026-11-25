cmake_minimum_required(VERSION 3.21)
project(bosskey LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON) # the controller and model are Q_OBJECT types

# The same split as WinShun's own winshun.capture (src/capture-plugin):
# bosskey.exe is a starter without Qt (qmlapp/stub/main.cpp); it loads
# BossKeyApp.dll, built on WinShun's Qt (WS_APP_DIR), which runs the plugin
# protocol on a worker thread and the fullscreen QML cover on the GUI thread.
# plugin.json names bosskey.exe.
set(WINSHUN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../WinShun" CACHE PATH
    "The WinShun checkout the shared platform helpers come from")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick ShaderToolsTools)
# The QML module's resources under :/qt/qml/<URI>, as WinShun's own app does.
qt_policy(SET QTP0001 NEW)

add_executable(bosskey qmlapp/stub/main.cpp bosskey.rc)
add_executable(bosskey-guard WIN32 qmlapp/guard/main.cpp qmlapp/input/KeyboardHook.cpp bosskey.rc)
target_compile_definitions(bosskey-guard PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN BOSSKEY_INPUT_GUARD)
target_link_libraries(bosskey-guard PRIVATE user32 advapi32 shell32 ole32)
set_target_properties(bosskey-guard PROPERTIES AUTOMOC OFF RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(bosskey-guard PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_SOURCE_DIR}")
endforeach()
if(MSVC)
    target_compile_options(bosskey-guard PRIVATE /utf-8 /W4 /permissive-)
    set_target_properties(bosskey-guard PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(bosskey-guard PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wextra)
    target_link_options(bosskey-guard PRIVATE -static -static-libgcc -static-libstdc++ -municode)
endif()
target_compile_definitions(bosskey PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
# Beside BossKeyApp.dll, where plugin.json's runtime.program points.
set_target_properties(bosskey PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(bosskey PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_SOURCE_DIR}")
endforeach()
if(MSVC)
    target_compile_options(bosskey PRIVATE /utf-8 /W4 /permissive-)
    # Static CRT: the exe needs no runtime DLL, which the strict sandbox cannot load.
    set_target_properties(bosskey PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(bosskey PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wextra -O2)
    target_link_options(bosskey PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# The plugin itself, built on WinShun's Qt: the query/action mapping (main.cpp),
# the QML cover (qmlapp), and the shared platform helpers the cover needs
# (bringToFront for the focus after Enter, see BossQml.cpp).
set(app_dir "${WINSHUN_DIR}/src/app")
set(core_dir "${WINSHUN_DIR}/src/core")
qt_add_library(BossKeyApp SHARED
    main.cpp
    bosskey-icons.qrc
    qmlapp/qmlmain.cpp
    qmlapp/BossQml.cpp
    qmlapp/CoverInput.cpp
    qmlapp/EnhancedInput.cpp
    qmlapp/input/KeyboardHook.cpp
    qmlapp/CoverController.cpp
    qmlapp/GccModel.cpp
    "${app_dir}/platform/WindowEffects.cpp" "${app_dir}/platform/WindowEffects.h"
    "${app_dir}/platform/Foreground.cpp" "${app_dir}/platform/Foreground.h"
    "${core_dir}/Win32Util.cpp" "${core_dir}/Win32Util.h" "${core_dir}/OwnInput.h"
)
target_compile_definitions(BossKeyApp PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
# WinShun's shared app sources are C++20, as its own CMakeLists builds them.
set_target_properties(BossKeyApp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
set_source_files_properties(main.cpp qmlapp/qmlmain.cpp qmlapp/BossQml.cpp
    PROPERTIES COMPILE_OPTIONS "/std:c++17")
target_include_directories(BossKeyApp PRIVATE
    "${CMAKE_SOURCE_DIR}" "${app_dir}" "${app_dir}/platform" "${core_dir}")

# The QML module URI BossKey: the plugin's own scenes only. The shared
# WinShun controls are not used — a fake OS screen borrows nothing from them.
set(bosskey_qml
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/BossCover.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/BluescreenScene.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/UpdateScene.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/GccScene.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/MatrixScene.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/BlackholeScene.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/AquariumScene.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml/Aquarium.js")
foreach(file IN LISTS bosskey_qml)
    get_filename_component(name "${file}" NAME)
    set_source_files_properties("${file}" PROPERTIES QT_RESOURCE_ALIAS "${name}")
endforeach()
set(qml_import_scan "")
if(QT6_IS_SHARED_LIBS_BUILD)
    set(qml_import_scan NO_IMPORT_SCAN) # as WinShun's (src/app/CMakeLists.txt)
endif()
qt_add_qml_module(BossKeyApp
    URI BossKey
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/BossKey"
    VERSION 1.0
    NO_PLUGIN
    ${qml_import_scan}
    NO_GENERATE_EXTRA_QMLDIRS
    QML_FILES ${bosskey_qml}
)
# Bytecode only, no QML compiled to C++: the plugin runs on whichever Qt 6 its
# WinShun comes with (see capture's CMakeLists).
set_target_properties(BossKeyApp PROPERTIES QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode")

# The black hole scene's fragment shader: qsb bakes it into the resource tree at
# the QML module's own prefix, so the scene's relative URL
# "BlackholeScene.frag.qsb" resolves. Default bake: GLSL 100es/120/150,
# HLSL 5.0, MSL 1.2.
qt_add_shaders(BossKeyApp "bosskey_shaders"
    PREFIX "/qt/qml/BossKey"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qmlapp/qml"
    FILES "qmlapp/qml/BlackholeScene.frag")

target_link_libraries(BossKeyApp PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick
    user32 gdi32 dwmapi advapi32 ole32 shell32 shlwapi uxtheme comctl32 imm32 shcore)
set_target_properties(BossKeyApp PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(BossKeyApp PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_SOURCE_DIR}")
endforeach()

if(MSVC)
    target_compile_options(BossKeyApp PRIVATE /utf-8 /W3)
endif()

# Pure-logic tests: logic.h has no OS dependency, so it builds and runs on any
# C++17 compiler without starting a plugin process or opening a window.
add_executable(bosskey_tests tests/test_logic.cpp)
target_include_directories(bosskey_tests PRIVATE "${CMAKE_SOURCE_DIR}")
if(MSVC)
    target_compile_options(bosskey_tests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(bosskey_tests PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wextra -O2)
endif()
set_target_properties(bosskey_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(bosskey_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/tests")
endforeach()

enable_testing()
add_test(NAME bosskey_logic COMMAND bosskey_tests)

add_executable(bosskey_input_tests tests/test_input.cpp)
target_include_directories(bosskey_input_tests PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_definitions(bosskey_input_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(bosskey_input_tests PRIVATE advapi32)
set_target_properties(bosskey_input_tests PROPERTIES AUTOMOC OFF)
if(MSVC)
    target_compile_options(bosskey_input_tests PRIVATE /utf-8 /W4 /permissive-)
endif()
add_test(NAME bosskey_input COMMAND bosskey_input_tests)
