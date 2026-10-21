add_subdirectory(common)

if(CLORELA_BUILD_ACQD)
    add_subdirectory(hw)
    add_subdirectory(status)
    add_subdirectory(acqd)
endif()

if(CLORELA_BUILD_UI)
    add_subdirectory(ui)
endif()
