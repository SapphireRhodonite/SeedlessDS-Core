target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/bg_affine.c
    ${CMAKE_CURRENT_LIST_DIR}/bg_text.c
    ${CMAKE_CURRENT_LIST_DIR}/blend.c
    ${CMAKE_CURRENT_LIST_DIR}/compose.c
    ${CMAKE_CURRENT_LIST_DIR}/deferred.c
    ${CMAKE_CURRENT_LIST_DIR}/gpu2d.c
    ${CMAKE_CURRENT_LIST_DIR}/line.c
    ${CMAKE_CURRENT_LIST_DIR}/obj.c
    ${CMAKE_CURRENT_LIST_DIR}/palette.c
    ${CMAKE_CURRENT_LIST_DIR}/planes.c
    ${CMAKE_CURRENT_LIST_DIR}/worker.c
)
