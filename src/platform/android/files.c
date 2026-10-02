#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include <stddef.h>
#include <string.h>
#include "frontend/frontend.h"
#include <jni.h>
#include "mem_access.h"

static void (*core_free)(void *);

static void free_core(void *p) {
    if (!core_free) core_free = (void (*)(void *))sym_libc_free;
    core_free(p);
}

void files_descriptor_free(void **obj) {
    free_core(obj[0]);
    free_core(obj[2]);
    free_core(obj);
}

static void *(*core_malloc)(size_t);
static char *(*core_strdup)(const char *);

static void *slot(void *env, int n) {
    void **table = *(void ***)env;
    return table[n];
}

void *files_jni_call_static_method_build_result(const char *arg0, const char *arg1) {
    if (!core_malloc) {
        core_malloc = (void *(*)(size_t))sym_libc_malloc;
        core_strdup = (char *(*)(const char *))sym_libc_strdup;
    }
    void **slot_vm = &FRONTEND->jni.vm;
    void **fields    = &FRONTEND->jni.core_method_a;
    void **class_ref  = &FRONTEND->jni.files_class;

    void *vm = *slot_vm;
    void *env = 0;
    int lo_hook = 0;

    {
        int (*get)(void *, void **, int) =
            (int (*)(void *, void **, int))slot(vm, 6);
        int rc = get(vm, &env, 0x10006);

        if (rc == -2) {
            void *vm2 = *slot_vm;
            int (*hook)(void *, void **, void *) =
                (int (*)(void *, void **, void *))slot(vm2, 4);
            hook(vm2, &env, 0);
            lo_hook = 1;
        }
    }

    void *(*new_str)(void *, const char *) = (void *(*)(void *, const char *))slot(env, 167);
    void *s0 = new_str(env, arg0);
    void *s1 = new_str(env, arg1);

    void *(*call)(void *, void *, void *, void *, void *) =
        (void *(*)(void *, void *, void *, void *, void *))slot(env, 114);
    void *obj = call(env, class_ref[0], class_ref[1], s0, s1);

    void *res = 0;
    if (obj != 0) {
        void *(*field_obj)(void *, void *, void *) =
            (void *(*)(void *, void *, void *))slot(env, 95);
        void *(*open_str)(void *, void *, void *) =
            (void *(*)(void *, void *, void *))slot(env, 169);
        int  (*field_int)(void *, void *, void *) =
            (int (*)(void *, void *, void *))slot(env, 100);
        void (*close_str)(void *, void *, void *) =
            (void (*)(void *, void *, void *))slot(env, 170);

        void *j0 = field_obj(env, obj, fields[0]);
        void *c0 = open_str(env, j0, 0);
        int   n1 = field_int(env, obj, fields[1]);
        void *j2 = field_obj(env, obj, fields[2]);
        void *c2 = open_str(env, j2, 0);

        unsigned char *r = (unsigned char *)core_malloc(24);
        *(void **)(r + 0)  = core_strdup((const char *)c0);
        *(uint32_t *)(r + 8) = (uint32_t)n1;
        *(void **)(r + 16) = core_strdup((const char *)c2);
        res = r;


        close_str(env, j2, c2);
        close_str(env, j0, c0);

        void (*release)(void *, void *) = (void (*)(void *, void *))slot(env, 23);
        release(env, obj);
    }

    void (*release)(void *, void *) = (void (*)(void *, void *))slot(env, 23);
    release(env, s1);
    release(env, s0);

    if (lo_hook) {
        void *vm2 = *slot_vm;
        void (*unhook)(void *) = (void (*)(void *))slot(vm2, 5);
        unhook(vm2);
    }
    return res;
}


uint32_t files_jni_env_invoke_and_release(const char *arg0, const char *arg1)
{

    JNIEnv *local = 0;
    JavaVM *root = FRONTEND->jni.vm;
    int32_t state = (*root)->GetEnv(root, (void **)&local, 0x10006u);
    JNIEnv *ctx = state == -2 ? 0 : local;
    uint32_t tmp = ctx == 0;

    if (tmp) {
        root = FRONTEND->jni.vm;
        (*root)->AttachCurrentThread(root, &local, 0);
        ctx = local;
    }

    jstring first = (*ctx)->NewStringUTF(ctx, arg0);
    jstring second = (*ctx)->NewStringUTF(ctx, arg1);
    jclass value1 = FRONTEND->jni.files_class;
    jmethodID value2 = FRONTEND->jni.files_method_b;
    jboolean result = (*ctx)->CallStaticBooleanMethod(
        ctx, value1, value2, first, second);

    (*ctx)->DeleteLocalRef(ctx, first);
    (*ctx)->DeleteLocalRef(ctx, second);

    if (tmp) {
        root = FRONTEND->jni.vm;
        (*root)->DetachCurrentThread(root);
    }

    return result == 0;
}



uint32_t files_jni_env_invoke_alternate(void *arg)
{

    JavaVM *manager = FRONTEND->jni.vm;
    JNIEnv *obj = 0;

    int32_t state = (*manager)->GetEnv(manager, (void **)&obj, 0x10006u);

    if (state == -2)
        obj = 0;

    uint32_t alternate = (obj == 0);
    if (alternate) {

        manager = FRONTEND->jni.vm;
        (*manager)->AttachCurrentThread(manager, &obj, 0);
    }

    jstring resource = (*obj)->NewStringUTF(obj, arg);

    jboolean result = (*obj)->CallStaticBooleanMethod(obj, FRONTEND->jni.files_class,
                                                      FRONTEND->jni.files_method_b, resource);

    (*obj)->DeleteLocalRef(obj, resource);

    if (alternate) {

        manager = FRONTEND->jni.vm;
        (*manager)->DetachCurrentThread(manager);
    }

    return result == 0;
}

