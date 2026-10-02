target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/adpcm.S
    ${CMAKE_CURRENT_LIST_DIR}/adpcm_tables.c
    ${CMAKE_CURRENT_LIST_DIR}/mixer.c
    ${CMAKE_CURRENT_LIST_DIR}/resample.c
    ${CMAKE_CURRENT_LIST_DIR}/spu.c
)
