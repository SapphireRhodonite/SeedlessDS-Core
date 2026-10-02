target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/files.c
    ${CMAKE_CURRENT_LIST_DIR}/libc_shims.c
    ${CMAKE_CURRENT_LIST_DIR}/platform.c
    ${CMAKE_CURRENT_LIST_DIR}/threads.c
    ${CMAKE_CURRENT_LIST_DIR}/time.c
)
