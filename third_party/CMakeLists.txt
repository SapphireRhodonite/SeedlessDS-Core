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

set(SEEDLESS_UNRAR_URL    "https://www.rarlab.com/rar/unrarsrc-5.0.14.tar.gz")
set(SEEDLESS_UNRAR_SHA256 "e276ea30a2dc9b8961a4268772e5b6caccaa984664e2d5255a43fcb0f5d59806")
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

seedless_fetch(unrar "${SEEDLESS_UNRAR_URL}" "${SEEDLESS_UNRAR_SHA256}" unrarsrc-5.0.14.tar.gz unrar-5.0.14)
set(SEEDLESS_UNRAR_DIR "${SEEDLESS_THIRD_PARTY_DIR}/unrar-5.0.14/unrar")
seedless_rewrite("${SEEDLESS_UNRAR_DIR}/file.cpp"
    "#include \"rar.hpp\"\n"
    "#include \"rar.hpp\"\nextern \"C\" int recon_archive_open(const char *path, int flags);\nextern \"C\" FILE *recon_archive_fopen(const char *path, const char *mode);\n")
seedless_rewrite("${SEEDLESS_UNRAR_DIR}/file.cpp"
    "  int handle=open(NameA,flags);"
    "  int handle=recon_archive_open(NameA,flags);")
seedless_rewrite("${SEEDLESS_UNRAR_DIR}/file.cpp"
    "  hFile=fopen(NameA,WriteMode ? WRITEBINARY:CREATEBINARY);"
    "  hFile=recon_archive_fopen(NameA,WriteMode ? WRITEBINARY:CREATEBINARY);")

seedless_rewrite("${SEEDLESS_UNRAR_DIR}/unicode.cpp"
    "#define MBFUNCTIONS\n"
    "#undef MBFUNCTIONS\n")
seedless_rewrite("${SEEDLESS_UNRAR_DIR}/unicode.cpp"
    "#elif defined(_APPLE)\n  WideToUtf(Src,Dest,DestSize);"
    "#elif defined(_APPLE) || defined(__ANDROID__)\n  WideToUtf(Src,Dest,DestSize);")
seedless_rewrite("${SEEDLESS_UNRAR_DIR}/unicode.cpp"
    "#elif defined(_APPLE)\n  UtfToWide(Src,Dest,DestSize);"
    "#elif defined(_APPLE) || defined(__ANDROID__)\n  UtfToWide(Src,Dest,DestSize);")
set(SEEDLESS_UNRAR_OBJ rar strlist strfn pathfn smallfn global file filefn filcreat
    archive arcread unicode system isnt crypt crc rawread encname
    resource match timefn rdwrfn consio options errhnd rarvm secpassword
    rijndael getbits sha1 sha256 blake2s hash extinfo extract volume
    list find unpack headers threadpool rs16 cmddata
    filestr scantree dll qopen)
set(SEEDLESS_UNRAR_SRC)
foreach(o ${SEEDLESS_UNRAR_OBJ})
    list(APPEND SEEDLESS_UNRAR_SRC "${SEEDLESS_UNRAR_DIR}/${o}.cpp")
endforeach()
add_library(seedless_unrar STATIC ${SEEDLESS_UNRAR_SRC})

target_compile_definitions(seedless_unrar PRIVATE RARDLL RAR_SMP _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE LITTLE_ENDIAN=1234)
target_compile_options(seedless_unrar PRIVATE -O2 -fPIC -w -fexceptions -frtti)
target_include_directories(seedless_unrar PUBLIC "${SEEDLESS_UNRAR_DIR}")
set_target_properties(seedless_unrar PROPERTIES CXX_VISIBILITY_PRESET hidden)

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
