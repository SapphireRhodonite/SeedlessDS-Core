target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/io7.c
    ${CMAKE_CURRENT_LIST_DIR}/io9.c
    ${CMAKE_CURRENT_LIST_DIR}/io_common.c
    ${CMAKE_CURRENT_LIST_DIR}/mirror.c
    ${CMAKE_CURRENT_LIST_DIR}/vram.c
    ${CMAKE_CURRENT_LIST_DIR}/wram.c
    ${CMAKE_CURRENT_LIST_DIR}/input_shared_region_create.c
)
