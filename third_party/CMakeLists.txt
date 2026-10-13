cmake_minimum_required(VERSION 3.18)

set(SEEDLESS_LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua")
if(NOT EXISTS "${SEEDLESS_LUA_DIR}/lapi.c")
    message(FATAL_ERROR "third_party/lua is empty: git submodule update --init third_party/lua (v5.3.0)")
endif()
file(GLOB SEEDLESS_LUA_SRC "${SEEDLESS_LUA_DIR}/*.c")
list(REMOVE_ITEM SEEDLESS_LUA_SRC "${SEEDLESS_LUA_DIR}/lua.c" "${SEEDLESS_LUA_DIR}/luac.c"
     "${SEEDLESS_LUA_DIR}/onelua.c")
add_library(seedless_lua STATIC ${SEEDLESS_LUA_SRC})

set_source_files_properties("${SEEDLESS_LUA_DIR}/lauxlib.c" PROPERTIES COMPILE_DEFINITIONS "fopen=recon_lua_fopen")
target_compile_definitions(seedless_lua PUBLIC LUA_32BITS LUA_COMPAT_5_2)
target_compile_options(seedless_lua PRIVATE -O2 -fPIC -w)
target_include_directories(seedless_lua PUBLIC "${SEEDLESS_LUA_DIR}")
set_target_properties(seedless_lua PROPERTIES C_VISIBILITY_PRESET hidden)

set(SEEDLESS_LIBARCHIVE_URL "https://www.libarchive.org/downloads/libarchive-3.8.9.tar.xz")
set(SEEDLESS_LIBARCHIVE_SHA256 "888c934f9d95648ecb9163dc8e23ab80a476ecb81a8f1154704a227b5b676dde")
set(SEEDLESS_UNARR_URL "https://github.com/selmf/unarr/releases/download/v1.1.1/unarr-1.1.1.tar.xz")
set(SEEDLESS_UNARR_SHA256 "328efa04ea996dd38916b1247a8cf176ac29b85caf92174e34d31998de4174d2")
set(SEEDLESS_LZMA_URL     "https://www.7-zip.org/a/lzma920.tar.bz2")
set(SEEDLESS_LZMA_SHA256  "8ac221acdca8b6f6dd110120763af42b3707363752fc04e63c7bbff76774a445")
if(NOT SEEDLESS_THIRD_PARTY_DIR)
    set(SEEDLESS_THIRD_PARTY_DIR "${CMAKE_CURRENT_BINARY_DIR}/downloads")
endif()

function(seedless_fetch name url sha256 tarball outdir)
    if(NOT EXISTS "${SEEDLESS_THIRD_PARTY_DIR}/${tarball}")
        message(STATUS "third_party/${name}: downloading ${url}")
        file(DOWNLOAD "${url}" "${SEEDLESS_THIRD_PARTY_DIR}/${tarball}"
             EXPECTED_HASH "SHA256=${sha256}" STATUS st TLS_VERIFY ON)
        list(GET st 0 code)
        if(NOT code EQUAL 0)
            file(REMOVE "${SEEDLESS_THIRD_PARTY_DIR}/${tarball}")
            message(FATAL_ERROR "third_party/${name}: could not download ${url}: ${st}")
        endif()
    else()
        file(SHA256 "${SEEDLESS_THIRD_PARTY_DIR}/${tarball}" have)
        if(NOT have STREQUAL sha256)
            message(FATAL_ERROR "third_party/${name}: ${tarball} does not match the pinned SHA-256 (${have})")
        endif()
    endif()
    if(NOT EXISTS "${SEEDLESS_THIRD_PARTY_DIR}/${outdir}/.extracted")
        file(REMOVE_RECURSE "${SEEDLESS_THIRD_PARTY_DIR}/${outdir}")
        file(ARCHIVE_EXTRACT INPUT "${SEEDLESS_THIRD_PARTY_DIR}/${tarball}"
             DESTINATION "${SEEDLESS_THIRD_PARTY_DIR}/${outdir}")
        file(WRITE "${SEEDLESS_THIRD_PARTY_DIR}/${outdir}/.extracted" "${sha256}\n")
    endif()
endfunction()

function(seedless_rewrite path old new)
    file(READ "${path}" src)
    string(FIND "${src}" "${new}" pos2)
    if(NOT pos2 EQUAL -1)
        return()
    endif()
    string(FIND "${src}" "${old}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "third_party: ${path} does not contain '${old}'")
    endif()
    string(REPLACE "${old}" "${new}" src "${src}")
    file(WRITE "${path}" "${src}")
endfunction()

seedless_fetch(libarchive "${SEEDLESS_LIBARCHIVE_URL}" "${SEEDLESS_LIBARCHIVE_SHA256}" libarchive-3.8.9.tar.xz libarchive-3.8.9)
set(SEEDLESS_LIBARCHIVE_DIR "${SEEDLESS_THIRD_PARTY_DIR}/libarchive-3.8.9/libarchive-3.8.9")
function(seedless_add_libarchive)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    foreach(feature MBEDTLS NETTLE OPENSSL LIBB2 LZ4 LZO LZMA ZSTD BZip2 LIBXML2 EXPAT
                    WIN32_XMLLITE PCREPOSIX PCRE2POSIX LIBGCC CNG TAR CPIO CAT UNZIP
                    XATTR ACL ICONV TEST COVERAGE INSTALL)
        set(ENABLE_${feature} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(ENABLE_ZLIB ON CACHE BOOL "" FORCE)
    add_subdirectory("${SEEDLESS_LIBARCHIVE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libarchive" EXCLUDE_FROM_ALL)
    get_target_property(archive_sources archive_static SOURCES)
    set(reader_sources)
    foreach(source IN LISTS archive_sources)
        if(source MATCHES "(^|/)archive_write" OR source MATCHES "(^|/)archive_read_disk")
            continue()
        endif()
        if(source MATCHES "(^|/)archive_read_support_format_" AND
           NOT source MATCHES "archive_read_support_format_rar5?\\.c$")
            continue()
        endif()
        if(source MATCHES "(^|/)archive_read_support_filter_" AND
           NOT source MATCHES "archive_read_support_filter_none\\.c$")
            continue()
        endif()
        list(APPEND reader_sources "${source}")
    endforeach()
    set_target_properties(archive_static PROPERTIES SOURCES "${reader_sources}")
    set_target_properties(archive_static PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
    target_include_directories(archive_static INTERFACE "${SEEDLESS_LIBARCHIVE_DIR}/libarchive")
    target_compile_options(archive_static PRIVATE -ffunction-sections -fdata-sections)
endfunction()
seedless_add_libarchive()
add_library(seedless_libarchive ALIAS archive_static)

seedless_fetch(unarr "${SEEDLESS_UNARR_URL}" "${SEEDLESS_UNARR_SHA256}" unarr-1.1.1.tar.xz unarr-1.1.1)
set(SEEDLESS_UNARR_DIR "${SEEDLESS_THIRD_PARTY_DIR}/unarr-1.1.1/unarr-1.1.1")
seedless_rewrite("${SEEDLESS_UNARR_DIR}/common/stream.c"
    "#include \"unarr-imp.h\""
    "#include \"unarr-imp.h\"\nextern FILE *recon_archive_fopen(const char *, const char *);")
seedless_rewrite("${SEEDLESS_UNARR_DIR}/common/stream.c"
    "path ? fopen(path, \"rb\")" "path ? recon_archive_fopen(path, \"rb\")")
seedless_rewrite("${SEEDLESS_UNARR_DIR}/rar/rar.c"
    "if (!rar_check_header_crc(ar))\n                warn(\"Invalid header checksum @%\" PRIi64, ar->entry_offset);"
    "if (!rar_check_header_crc(ar)) {\n                warn(\"Invalid header checksum @%\" PRIi64, ar->entry_offset);\n                return false;\n            }")
seedless_rewrite("${SEEDLESS_UNARR_DIR}/rar/rar.c"
    "if (!rar_check_header_crc(ar))\n            warn(\"Invalid header checksum @%\" PRIi64, ar->entry_offset);"
    "if (!rar_check_header_crc(ar)) {\n            warn(\"Invalid header checksum @%\" PRIi64, ar->entry_offset);\n            return false;\n        }")
seedless_rewrite("${SEEDLESS_UNARR_DIR}/rar/rar.c"
    "if ((header.flags & LHD_PASSWORD))\n                warn(\"Encrypted entries will fail to uncompress\");"
    "if ((header.flags & LHD_PASSWORD)) {\n                warn(\"Encrypted entries are unsupported\");\n                return false;\n            }")
set(SEEDLESS_UNARR_SRC)
foreach(source common/conv.c common/crc32.c common/stream.c common/unarr.c
               lzmasdk/Ppmd7.c lzmasdk/Ppmd7aDec.c
               rar/filter-rar.c rar/uncompress-rar.c rar/huffman-rar.c
               rar/rar.c rar/rarvm.c rar/parse-rar.c)
    list(APPEND SEEDLESS_UNARR_SRC "${SEEDLESS_UNARR_DIR}/${source}")
endforeach()
add_library(seedless_unarr STATIC ${SEEDLESS_UNARR_SRC})
target_compile_definitions(seedless_unarr PRIVATE _POSIX_C_SOURCE=200809L NDEBUG HAVE_ZLIB USE_ZLIB_CRC)
target_compile_options(seedless_unarr PRIVATE -O2 -fPIC -ffunction-sections -fdata-sections)
target_link_libraries(seedless_unarr PRIVATE z)
target_include_directories(seedless_unarr PUBLIC "${SEEDLESS_UNARR_DIR}")
set_target_properties(seedless_unarr PROPERTIES C_VISIBILITY_PRESET hidden)

seedless_fetch(lzma "${SEEDLESS_LZMA_URL}" "${SEEDLESS_LZMA_SHA256}" lzma920.tar.bz2 lzma-9.20)
set(SEEDLESS_LZMA_DIR "${SEEDLESS_THIRD_PARTY_DIR}/lzma-9.20/C")
seedless_rewrite("${SEEDLESS_LZMA_DIR}/7zFile.c"
    "#include <errno.h>\n"
    "#include <errno.h>\nextern FILE *recon_archive_fopen(const char *path, const char *mode);\n")
seedless_rewrite("${SEEDLESS_LZMA_DIR}/7zFile.c"
    "  p->file = fopen(name, writeMode ? \"wb+\" : \"rb\");"
    "  p->file = recon_archive_fopen(name, writeMode ? \"wb+\" : \"rb\");")
set(SEEDLESS_LZMA_OBJ 7zAlloc 7zBuf 7zCrc 7zCrcOpt 7zDec 7zFile 7zIn 7zStream
    Bcj2 Bra Bra86 Lzma2Dec LzmaDec)
set(SEEDLESS_LZMA_SRC)
foreach(o ${SEEDLESS_LZMA_OBJ})
    list(APPEND SEEDLESS_LZMA_SRC "${SEEDLESS_LZMA_DIR}/${o}.c")
endforeach()
add_library(seedless_lzma STATIC ${SEEDLESS_LZMA_SRC})
target_compile_definitions(seedless_lzma PRIVATE _7ZIP_ST)
target_compile_options(seedless_lzma PRIVATE -O2 -fPIC -w)
target_include_directories(seedless_lzma PUBLIC "${SEEDLESS_LZMA_DIR}")
set_target_properties(seedless_lzma PROPERTIES C_VISIBILITY_PRESET hidden)
