# Shield status display (SSD1306 OLED over I2C).
qt_add_library(clorela_status STATIC
    font5x7.h
    framebuffer.h framebuffer.cpp
    statusdisplay.h statusdisplay.cpp
)
target_include_directories(clorela_status PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clorela_status PUBLIC clorela_common clorela_hw)

qt_add_executable(clorela-status main.cpp)
target_link_libraries(clorela-status PRIVATE clorela_status)

install(TARGETS clorela-status RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
