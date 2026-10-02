# Third-party code and its licenses

Lua is a Git submodule. UnRAR and LZMA SDK are downloaded at configure time from
the pinned URLs and verified with SHA-256 by [CMakeLists.txt](CMakeLists.txt).
Their archives and extracted sources are stored in the build directory under
`third_party/downloads` by default; `SEEDLESS_THIRD_PARTY_DIR` overrides that location.

| Library | Version | Source | License |
|---|---|---|---|
| Lua | 5.3.0 (`v5.3.0`) | github.com/lua/lua, submodule `third_party/lua` | MIT (`third_party/lua/lua.h`, end of file) |
| UnRAR | 5.0.14 | https://www.rarlab.com/rar/unrarsrc-5.0.14.tar.gz | UnRAR license (`unrar/license.txt` in the tarball): the source may be used in software that only decompresses RAR archives; it may not be used to build a RAR-compatible archiver. This core only decompresses. |
| LZMA SDK | 9.20 | https://www.7-zip.org/a/lzma920.tar.bz2 | Public domain (`lzma.txt` in the tarball); only the decoder side is built |
| zlib | system `libz.so` of Android | NDK sysroot | zlib license |

Local modifications, applied by `CMakeLists.txt` on the extracted copies and never committed:
`unrar/file.cpp` (open/fopen through the core's file resolver), `unrar/unicode.cpp` (UTF-8
name conversion, no `MBFUNCTIONS`), `lzma/C/7zFile.c` (fopen through the resolver),
`lua/lauxlib.c` (`fopen` renamed by a compile definition, no text change).
