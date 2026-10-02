target_include_directories(recon_fn PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dma.c
    ${CMAKE_CURRENT_LIST_DIR}/ipc.c
    ${CMAKE_CURRENT_LIST_DIR}/math.c
    ${CMAKE_CURRENT_LIST_DIR}/rtc.c
    ${CMAKE_CURRENT_LIST_DIR}/timers.c
)
include(${CMAKE_CURRENT_LIST_DIR}/cart/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/core/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/cpu/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/cpu/jit-arm64/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/debug/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/frontend/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/frontend/archive/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/frontend/cheats/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/frontend/script/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/gpu/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/gpu/gpu2d/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/gpu/gpu3d/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/gpu/gpu3d/raster/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/memory/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/platform/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/platform/android/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/spi/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/spu/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/util/CMakeLists.txt)
