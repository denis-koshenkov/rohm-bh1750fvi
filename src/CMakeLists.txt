add_library(driver INTERFACE)

target_sources(driver INTERFACE
    bh1750.c
)

target_include_directories(driver INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
