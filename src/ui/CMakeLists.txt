# Everything except main() lives in a static library so the tests can drive
# the real MainWindow.
qt_add_library(clorela_ui_core STATIC
    mainwindow.h mainwindow.cpp mainwindow.ui
    graphview.h graphview.cpp
    numericview.h numericview.cpp
    acqdclient.h acqdclient.cpp
)
target_include_directories(clorela_ui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clorela_ui_core PUBLIC clorela_common Qt6::Widgets Qt6::Network)

# English is the source language; Spanish and French are translations.
# The .qm files are embedded as resources under :/i18n/.
qt_add_translations(clorela_ui_core
    TS_FILES
        translations/clorela_es.ts
        translations/clorela_fr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete
)

qt_add_executable(clorela-ui main.cpp)
target_link_libraries(clorela-ui PRIVATE clorela_ui_core)

install(TARGETS clorela-ui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
