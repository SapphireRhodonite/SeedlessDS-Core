add_library(seedless_enet STATIC
    callbacks.c compress.c host.c list.c packet.c peer.c protocol.c unix.c)
target_include_directories(seedless_enet PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_definitions(seedless_enet PRIVATE
    HAS_FCNTL=1 HAS_POLL=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1
    HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
set_target_properties(seedless_enet PROPERTIES POSITION_INDEPENDENT_CODE ON)
