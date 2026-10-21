# Acquisition daemon. Qt Core + Sql + Network (local socket only); no Qt Gui.
qt_add_library(clorela_acq STATIC
    nodeprotocol.h
    nodetransport.h
    i2cnodetransport.h
    simulatednode.h simulatednode.cpp
    nodeclient.h nodeclient.cpp
    acquisitionservice.h acquisitionservice.cpp
    controlserver.h controlserver.cpp
)
target_include_directories(clorela_acq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clorela_acq PUBLIC clorela_common clorela_hw clorela_signer Qt6::Network)

qt_add_executable(clorela-acqd main.cpp)
target_link_libraries(clorela-acqd PRIVATE clorela_acq clorela_status)

install(TARGETS clorela-acqd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
