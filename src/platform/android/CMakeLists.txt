target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/audio.c
    ${CMAKE_CURRENT_LIST_DIR}/crash.c
    ${CMAKE_CURRENT_LIST_DIR}/files.c
    ${CMAKE_CURRENT_LIST_DIR}/JNI_OnLoad.c
    ${CMAKE_CURRENT_LIST_DIR}/JNI_OnUnload.c
)
