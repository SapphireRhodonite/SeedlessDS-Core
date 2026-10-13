# Third-party code and its licenses

Lua is a Git submodule. Other source archives are fetched from the pinned URLs and SHA-256 values in [CMakeLists.txt](CMakeLists.txt). Their original notices remain in the extracted source. UnRAR is no longer compiled or linked.

| Library | Version / origin | License and scope |
|---|---|---|
| Lua | 5.3.0, lua/lua submodule | MIT, full notice at the end of lua.h |
| libarchive | 3.8.9, https://www.libarchive.org/downloads/libarchive-3.8.9.tar.xz | BSD-2-Clause and per-file notices; BLAKE2 Apache-2.0 option selected; public-domain parts. Only RAR/RAR5 and the none filter are registered. |
| unarr | 1.1.1, https://github.com/selmf/unarr/releases/tag/v1.1.1 | LGPL-3.0; lzmasdk PPMd public domain. common/crc32.c currently declares LGPLv3; do not rely on the older AUTHORS summary for that file. Only common/RAR/PPMd are compiled. |
| LZMA SDK | 9.20, https://www.7-zip.org/a/lzma920.tar.bz2 | Public-domain selected C 7z/LZMA decoders; no RAR code. |
| zlib | Android system library, NDK 25.2.9519653 headers | zlib license; no new bundled binary. |

Local changes are reproducibly applied by CMake to downloaded copies: unarr/common/stream.c opens through the existing resolver; unarr/rar/rar.c rejects invalid header CRC and encrypted entries; 7zFile.c opens through the resolver. Lua lauxlib.c uses a compile definition for the resolver. unarr uses zlib CRC to avoid its mutable lazy lookup table. No optional archive compression/crypto dependencies are enabled beyond zlib.

The app packages complete license texts and attribution notices under assets/licenses and exposes them from Help. Corresponding source includes the fixed archives, submodule revisions and applied modifications. Static LGPL code can be rebuilt and relinked through the included source/build system.
