target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/bands.c
    ${CMAKE_CURRENT_LIST_DIR}/compose_band.c
    ${CMAKE_CURRENT_LIST_DIR}/depth_effects.c
    ${CMAKE_CURRENT_LIST_DIR}/interp.c
    ${CMAKE_CURRENT_LIST_DIR}/neon_helpers.c
    ${CMAKE_CURRENT_LIST_DIR}/span.c
    ${CMAKE_CURRENT_LIST_DIR}/texture.c
    ${CMAKE_CURRENT_LIST_DIR}/texture_palette_bank_fallback.c
    ${CMAKE_CURRENT_LIST_DIR}/workers.c
)
