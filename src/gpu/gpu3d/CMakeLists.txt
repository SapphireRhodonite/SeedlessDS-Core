target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dynamic_scale.c
    ${CMAKE_CURRENT_LIST_DIR}/geometry.c
    ${CMAKE_CURRENT_LIST_DIR}/gpu3d.c
    ${CMAKE_CURRENT_LIST_DIR}/gxfifo.c
    ${CMAKE_CURRENT_LIST_DIR}/gxfifo_state_primitive_table.c
    ${CMAKE_CURRENT_LIST_DIR}/io3d.c
    ${CMAKE_CURRENT_LIST_DIR}/matrix.c
    ${CMAKE_CURRENT_LIST_DIR}/poly.c
    ${CMAKE_CURRENT_LIST_DIR}/scale.c
)
