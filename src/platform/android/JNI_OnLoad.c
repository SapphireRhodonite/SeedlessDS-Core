#include "hires_runtime.h"
#include "blob_symbols.h"
#include "frontend/frontend.h"
#include "platform/android/jni_contract.h"

#include <jni.h>
#include <stdint.h>

#define CLASS_PATHS SEEDLESS_JNI_CLASS_PATHS
#define CLASS_CACHE SEEDLESS_JNI_CLASS_CACHE
#define SIG_OPEN  SEEDLESS_JNI_SIG_OPEN


jint JNI_OnLoad(JavaVM *vm, void *reserved)
{

    (void)reserved;

    JNIEnv *env = 0;
    jclass class;
    void *reference;

    FRONTEND->jni.vm = vm;
    (void)(*vm)->GetEnv(vm, (void **)&env, 0x10006u);

    class = (*env)->FindClass(env, CLASS_PATHS);

    reference = (*env)->NewGlobalRef(env, class);

    FRONTEND->jni.core_class = reference;
    reference = (*env)->GetFieldID(env, reference, SEEDLESS_JNI_FIELD_PATH,
        "Ljava/lang/String;");

    FRONTEND->jni.core_method_a = reference;
    class = FRONTEND->jni.core_class;
    reference = (*env)->GetFieldID(env, class, SEEDLESS_JNI_FIELD_FD, SEEDLESS_JNI_SIG_FD);

    FRONTEND->jni.core_method_b = reference;
    class = FRONTEND->jni.core_class;
    reference = (*env)->GetFieldID(env, class, SEEDLESS_JNI_FIELD_NAME, SEEDLESS_JNI_SIG_STRING);

    FRONTEND->jni.core_method_c = reference;
    class = (*env)->FindClass(env, CLASS_CACHE);

    reference = (*env)->NewGlobalRef(env, class);

    FRONTEND->jni.files_class = reference;
    reference = (*env)->GetStaticMethodID(env, reference, SEEDLESS_JNI_METHOD_OPEN,
        SIG_OPEN);

    FRONTEND->jni.files_method_a = reference;
    class = FRONTEND->jni.files_class;
    reference = (*env)->GetStaticMethodID(env, class, "rename", "(Ljava/lang/String;Ljava/lang/String;)Z");

    FRONTEND->jni.files_method_b = reference;
    class = FRONTEND->jni.files_class;
    reference = (*env)->GetStaticMethodID(env, class, "remove", "(Ljava/lang/String;)Z");
    FRONTEND->jni.files_method_c = reference;

    return 0x10006u;
}
