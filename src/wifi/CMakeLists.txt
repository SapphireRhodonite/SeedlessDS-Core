add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../third_party/enet
    ${CMAKE_CURRENT_BINARY_DIR}/enet)
add_library(seedless_wifi STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../../third_party/melonds_wifi/Wifi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../third_party/melonds_wifi/WifiFirmware.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../third_party/melonds_wifi/LAN.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wifi_platform.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wifi_session.cpp)
target_include_directories(seedless_wifi PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../third_party/melonds_wifi)
target_compile_features(seedless_wifi PRIVATE cxx_std_17)
target_link_libraries(seedless_wifi PRIVATE seedless_enet)
set_target_properties(seedless_wifi PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(ANDROID)
    target_link_libraries(seedless_wifi PRIVATE log)
endif()
if(TARGET recon_fn)
    target_sources(recon_fn PRIVATE ${CMAKE_CURRENT_LIST_DIR}/wifi_core.c
        ${CMAKE_CURRENT_LIST_DIR}/wm_console.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../third_party/melonds_wifi/WMService.cpp)
    target_include_directories(recon_fn PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../../third_party/melonds_wifi)
    target_compile_features(recon_fn PRIVATE cxx_std_17)
    target_link_libraries(recon_fn seedless_wifi)
endif()
