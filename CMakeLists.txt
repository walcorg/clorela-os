cmake_minimum_required(VERSION 3.21)

project(clorela-os
    VERSION 0.1.0
    DESCRIPTION "Clorela-OS: software for the WALCORG photobioreactor node"
    HOMEPAGE_URL "https://walcorg.com"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CLORELA_BUILD_UI    "Build the Qt Widgets interface (clorela-ui)" ON)
option(CLORELA_BUILD_ACQD  "Build the acquisition daemon (clorela-acqd)" ON)
option(CLORELA_BUILD_TESTS "Build the unit tests" ON)

set(_qt_components Core Sql Network)
if(CLORELA_BUILD_UI)
    list(APPEND _qt_components Gui Widgets LinguistTools)
endif()
if(CLORELA_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})
qt_standard_project_setup()

include(GNUInstallDirs)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

add_compile_definitions(
    CLORELA_VERSION="${PROJECT_VERSION}"
    QT_DISABLE_DEPRECATED_BEFORE=0x060400)

add_subdirectory(src)

if(CLORELA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# FULL_ form: with prefix /usr this is /etc, as on the node.
install(FILES data/clorela.conf DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/clorela)
