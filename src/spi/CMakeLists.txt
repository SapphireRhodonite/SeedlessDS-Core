target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/firmware.c
    ${CMAKE_CURRENT_LIST_DIR}/memory.c
    ${CMAKE_CURRENT_LIST_DIR}/powerman.c
    ${CMAKE_CURRENT_LIST_DIR}/spi.c
    ${CMAKE_CURRENT_LIST_DIR}/touchscreen.c
)
