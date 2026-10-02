#ifndef SEEDLESS_JNI_CONTRACT_H
#define SEEDLESS_JNI_CONTRACT_H

#define SEEDLESS_JNI_CLASS_CACHE "com/seedlessds/app/filesystem/SeedlessPathCache"
#define SEEDLESS_JNI_CLASS_PATHS "com/seedlessds/app/filesystem/NativePathHandle"
#define SEEDLESS_JNI_SIG_OPEN \
    "(Ljava/lang/String;Ljava/lang/String;)L" SEEDLESS_JNI_CLASS_PATHS ";"

#define SEEDLESS_JNI_METHOD_OPEN "open"
#define SEEDLESS_JNI_FIELD_FD    "fileFd"
#define SEEDLESS_JNI_FIELD_NAME "fileName"
#define SEEDLESS_JNI_FIELD_PATH  "filePath"

#define SEEDLESS_JNI_SIG_FD     "I"
#define SEEDLESS_JNI_SIG_STRING "Ljava/lang/String;"

#endif
