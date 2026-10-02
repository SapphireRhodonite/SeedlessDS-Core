#include "hires_runtime.h"
#include "blob_symbols.h"
#include "frontend/frontend.h"

#include <jni.h>
#include <stdint.h>


void JNI_OnUnload(JavaVM *vm, void *reserved)
{

    (void)vm;
    (void)reserved;

    frontend_jni_t *jni = &FRONTEND->jni;
    JavaVM *manager = jni->vm;
    JNIEnv *volatile obj_output;

    (*manager)->GetEnv(manager, (void **)&obj_output, 0x10006u);

    JNIEnv *object_first = obj_output;
    jobject resource_first = jni->core_class;
    (*object_first)->DeleteGlobalRef(object_first, resource_first);
    jni->core_class = 0;

    JNIEnv *obj_second = obj_output;
    jobject resource_second = jni->files_class;
    (*obj_second)->DeleteGlobalRef(obj_second, resource_second);
    jni->files_class = 0;
}
