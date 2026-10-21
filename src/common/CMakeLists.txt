# Code shared by the interface and the acquisition daemon. Qt Core + Sql only,
# so the daemon never links against Qt Gui.
qt_add_library(clorela_common STATIC
    config.h config.cpp
    measurement.h
    measurementstore.h measurementstore.cpp
    networkstatus.h networkstatus.cpp
)

target_include_directories(clorela_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clorela_common PUBLIC Qt6::Core Qt6::Sql)
