# Linux hardware access (i2c-dev). No Qt Gui.
qt_add_library(clorela_hw STATIC
    i2cdevice.h i2cdevice.cpp
)
target_include_directories(clorela_hw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clorela_hw PUBLIC Qt6::Core)
