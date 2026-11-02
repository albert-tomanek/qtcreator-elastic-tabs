cmake_minimum_required(VERSION 3.16)

project(ElasticTabstops LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt Creator does not ship a plugin SDK on all distributions (e.g. Debian), so
# this builds against the headers of a Qt Creator source checkout of the exact
# same version as the installed IDE and links its installed libraries directly.
set(QTC_VERSION "16.0.1" CACHE STRING "Version of the Qt Creator to build for")
set(QTC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtcreator-src"
    CACHE PATH "Qt Creator source checkout matching QTC_VERSION (for headers)")
set(QTC_LIB_DIR "/usr/lib/x86_64-linux-gnu/qtcreator"
    CACHE PATH "Directory with Qt Creator's libraries (libUtils.so...)")
set(QTC_PLUGIN_INSTALL_DIR
    "$ENV{HOME}/.local/share/data/QtProject/qtcreator/plugins/${QTC_VERSION}"
    CACHE PATH "Where 'cmake --install' puts the plugin")

option(BUILD_TESTING "Build the offscreen tests of the layout engine" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# The engine has no Qt Creator dependencies.
add_library(ElasticTabstopsEngine STATIC
    src/elastictabstops.h
    src/elastictabstops.cpp
)
target_include_directories(ElasticTabstopsEngine PUBLIC src)
target_link_libraries(ElasticTabstopsEngine PUBLIC Qt6::Widgets)
set_target_properties(ElasticTabstopsEngine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The Qt Creator plugin.
configure_file(src/ElasticTabstops.json.in ElasticTabstops.json @ONLY)

function(qtc_import_library name file)
    if (NOT EXISTS "${file}")
        message(FATAL_ERROR "Qt Creator library not found: ${file}")
    endif()
    add_library(${name} SHARED IMPORTED)
    set_target_properties(${name} PROPERTIES IMPORTED_LOCATION "${file}" IMPORTED_NO_SONAME OFF)
endfunction()

string(REGEX MATCH "^[0-9]+" QTC_VERSION_MAJOR "${QTC_VERSION}")
qtc_import_library(QtCreator::Utils "${QTC_LIB_DIR}/libUtils.so.${QTC_VERSION_MAJOR}")
qtc_import_library(QtCreator::ExtensionSystem "${QTC_LIB_DIR}/libExtensionSystem.so.${QTC_VERSION_MAJOR}")
qtc_import_library(QtCreator::Aggregation "${QTC_LIB_DIR}/libAggregation.so.${QTC_VERSION_MAJOR}")
qtc_import_library(QtCreator::Core "${QTC_LIB_DIR}/plugins/libCore.so")
qtc_import_library(QtCreator::TextEditor "${QTC_LIB_DIR}/plugins/libTextEditor.so")

add_library(ElasticTabstops MODULE src/elastictabstopsplugin.cpp)
target_include_directories(ElasticTabstops PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${QTC_SOURCE_DIR}/src/libs"
    "${QTC_SOURCE_DIR}/src/plugins"
)
target_link_libraries(ElasticTabstops PRIVATE
    ElasticTabstopsEngine
    QtCreator::TextEditor
    QtCreator::Core
    QtCreator::ExtensionSystem
    QtCreator::Utils
    QtCreator::Aggregation
    Qt6::Widgets
)
target_compile_definitions(ElasticTabstops PRIVATE QT_NO_CAST_TO_ASCII)
set_target_properties(ElasticTabstops PROPERTIES PREFIX "lib")

install(TARGETS ElasticTabstops LIBRARY DESTINATION "${QTC_PLUGIN_INSTALL_DIR}")

if (BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(tst_elastictabstops tests/tst_elastictabstops.cpp)
    target_link_libraries(tst_elastictabstops PRIVATE ElasticTabstopsEngine Qt6::Test)
    add_test(NAME tst_elastictabstops COMMAND tst_elastictabstops -platform offscreen)
endif()
