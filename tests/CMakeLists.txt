add_compile_definitions(CLORELA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

function(clorela_add_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${T_LIBS})
    add_test(NAME ${name} COMMAND ${name})
    # Widgets tests run without a display.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

clorela_add_test(tst_config SOURCES tst_config.cpp LIBS clorela_common)
clorela_add_test(tst_store SOURCES tst_store.cpp LIBS clorela_common)
clorela_add_test(tst_networkstatus SOURCES tst_networkstatus.cpp LIBS clorela_common)

if(CLORELA_BUILD_UI)
    clorela_add_test(tst_mainwindow SOURCES tst_mainwindow.cpp LIBS clorela_ui_core)
endif()

if(CLORELA_BUILD_ACQD)
    clorela_add_test(tst_framebuffer SOURCES tst_framebuffer.cpp LIBS clorela_status)
    clorela_add_test(tst_nodeclient SOURCES tst_nodeclient.cpp LIBS clorela_acq)
    clorela_add_test(tst_acquisition SOURCES tst_acquisition.cpp LIBS clorela_acq)
endif()
