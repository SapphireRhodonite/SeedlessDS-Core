target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/arm.c
    ${CMAKE_CURRENT_LIST_DIR}/bus.c
    ${CMAKE_CURRENT_LIST_DIR}/cp15.c
    ${CMAKE_CURRENT_LIST_DIR}/pagetable.c
)
