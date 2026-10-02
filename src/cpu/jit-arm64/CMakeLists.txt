target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/analyze.c
    ${CMAKE_CURRENT_LIST_DIR}/backend.c
    ${CMAKE_CURRENT_LIST_DIR}/cache.c
    ${CMAKE_CURRENT_LIST_DIR}/emit.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_alu_imm.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_alu_reg.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_branch.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_cop_ldc.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_cop_swi.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_ldm_stm.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_ldst_imm.c
    ${CMAKE_CURRENT_LIST_DIR}/emit_ldst_reg.c
    ${CMAKE_CURRENT_LIST_DIR}/memhooks.S
)
