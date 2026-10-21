# Signing extension point (TROPIC01). Only NullSigner exists for now.
qt_add_library(clorela_signer STATIC
    signer.h signer.cpp
)
target_include_directories(clorela_signer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clorela_signer PUBLIC clorela_common)
