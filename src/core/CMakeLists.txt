target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/benchmark.c
    ${CMAKE_CURRENT_LIST_DIR}/config.c
    ${CMAKE_CURRENT_LIST_DIR}/gamedb.c
    ${CMAKE_CURRENT_LIST_DIR}/nds.c
    ${CMAKE_CURRENT_LIST_DIR}/nds_api.c
    ${CMAKE_CURRENT_LIST_DIR}/nds_api_rom.c
    ${CMAKE_CURRENT_LIST_DIR}/nds_api_cheats.c
    ${CMAKE_CURRENT_LIST_DIR}/nds_api_fx.c
    ${CMAKE_CURRENT_LIST_DIR}/nds_api_video.c
    ${CMAKE_CURRENT_LIST_DIR}/pacer.c
    ${CMAKE_CURRENT_LIST_DIR}/sched.c
    ${CMAKE_CURRENT_LIST_DIR}/state.c
    ${CMAKE_CURRENT_LIST_DIR}/jni_system_store_formatted_message.c
    ${CMAKE_CURRENT_LIST_DIR}/system_reset_module_state.c
    ${CMAKE_CURRENT_LIST_DIR}/system_set_config_byte.c
)
