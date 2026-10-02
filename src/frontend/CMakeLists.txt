target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/audio_out.c
    ${CMAKE_CURRENT_LIST_DIR}/audio_out_silence.c
    ${CMAKE_CURRENT_LIST_DIR}/font_image.c
    ${CMAKE_CURRENT_LIST_DIR}/mic_default_sample.c
    ${CMAKE_CURRENT_LIST_DIR}/cheats.c
    ${CMAKE_CURRENT_LIST_DIR}/input.c
    ${CMAKE_CURRENT_LIST_DIR}/input_record.c
    ${CMAKE_CURRENT_LIST_DIR}/mic.c
    ${CMAKE_CURRENT_LIST_DIR}/shaders.c
    ${CMAKE_CURRENT_LIST_DIR}/video_out.c
    ${CMAKE_CURRENT_LIST_DIR}/video_out_gl.c
    ${CMAKE_CURRENT_LIST_DIR}/frontend.c
)
