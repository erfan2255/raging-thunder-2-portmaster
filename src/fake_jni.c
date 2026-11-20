#include "fake_jni.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MAX_CLASSES 128
#define MAX_METHODS 256
#define MAX_FIELDS  128
#define MAX_DIRECT_BUFFERS 128
#define MAX_ARRAYS  256

#define FAKE_OBJ_MAGIC 0x464A4E49u // 'FJNI'

typedef enum {
    FAKE_KIND_STRING = 1,
    FAKE_KIND_ARRAY  = 2,
    FAKE_KIND_BUFFER = 3
} FakeKind;

typedef struct {
    uint32_t magic;
    FakeKind kind;
    jsize length;
    size_t elem_size;
    void *data;
} FakeArray;

typedef struct {
    char name[128];
    uintptr_t id;
} FakeClass;

typedef struct {
    char name[128];
    char sig[160];
    uintptr_t class_id;
    uintptr_t id;
} FakeMethod;

typedef struct {
    char name[128];
    char sig[128];
    uintptr_t class_id;
    uintptr_t id;
} FakeField;

typedef struct {
    uint32_t magic;
    FakeKind kind;
    void *address;
    jlong capacity;
    int in_use;
} DirectBuffer;

static FakeClass g_classes[MAX_CLASSES];
static int g_class_count = 0;

static FakeMethod g_methods[MAX_METHODS];
static int g_method_count = 0;

static FakeField g_fields[MAX_FIELDS];
static int g_field_count = 0;

static DirectBuffer g_direct_buffers[MAX_DIRECT_BUFFERS];

static char g_apk_path[512] = "ragingthunder2.apk";
static char g_data_path[512] = "./gamedata/";
static void (*g_display_swap_cb)(void) = NULL;
static int (*g_audio_create_cb)(int freq, int channels, int bits, int buf_size) = NULL;
static int (*g_audio_start_cb)(void) = NULL;
static void (*g_audio_stop_cb)(void) = NULL;

void fake_jni_set_display_swap_cb(void (*cb)(void)) {
    g_display_swap_cb = cb;
}

void fake_jni_set_audio_callbacks(
    int (*on_create)(int freq, int channels, int bits, int buf_size),
    int (*on_start)(void),
    void (*on_stop)(void)
) {
    g_audio_create_cb = on_create;
    g_audio_start_cb = on_start;
    g_audio_stop_cb = on_stop;
}

static uintptr_t register_class(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < g_class_count; i++) {
        if (strcmp(g_classes[i].name, name) == 0) {
            return g_classes[i].id;
        }
    }
    if (g_class_count < MAX_CLASSES) {
        strncpy(g_classes[g_class_count].name, name, sizeof(g_classes[0].name) - 1);
        g_classes[g_class_count].name[sizeof(g_classes[0].name) - 1] = '\0';
        g_classes[g_class_count].id = (uintptr_t)(g_class_count + 100);
        return g_classes[g_class_count++].id;
    }
    return 1;
}

static uintptr_t register_method(uintptr_t class_id, const char *name, const char *sig) {
    if (!name) return 0;
    for (int i = 0; i < g_method_count; i++) {
        if (g_methods[i].class_id == class_id &&
            strcmp(g_methods[i].name, name) == 0 &&
            (!sig || strcmp(g_methods[i].sig, sig) == 0)) {
            return g_methods[i].id;
        }
    }
    if (g_method_count < MAX_METHODS) {
        strncpy(g_methods[g_method_count].name, name, sizeof(g_methods[0].name) - 1);
        g_methods[g_method_count].name[sizeof(g_methods[0].name) - 1] = '\0';
        if (sig) {
            strncpy(g_methods[g_method_count].sig, sig, sizeof(g_methods[0].sig) - 1);
            g_methods[g_method_count].sig[sizeof(g_methods[0].sig) - 1] = '\0';
        } else {
            g_methods[g_method_count].sig[0] = '\0';
        }
        g_methods[g_method_count].class_id = class_id;
        g_methods[g_method_count].id = (uintptr_t)(g_method_count + 1000);
        return g_methods[g_method_count++].id;
    }
    return 1;
}

static uintptr_t register_field(uintptr_t class_id, const char *name, const char *sig) {
    if (!name) return 0;
    for (int i = 0; i < g_field_count; i++) {
        if (g_fields[i].class_id == class_id &&
            strcmp(g_fields[i].name, name) == 0) {
            return g_fields[i].id;
        }
    }
    if (g_field_count < MAX_FIELDS) {
        strncpy(g_fields[g_field_count].name, name, sizeof(g_fields[0].name) - 1);
        g_fields[g_field_count].name[sizeof(g_fields[0].name) - 1] = '\0';
        if (sig) {
            strncpy(g_fields[g_field_count].sig, sig, sizeof(g_fields[0].sig) - 1);
            g_fields[g_field_count].sig[sizeof(g_fields[0].sig) - 1] = '\0';
        } else {
            g_fields[g_field_count].sig[0] = '\0';
        }
        g_fields[g_field_count].class_id = class_id;
        g_fields[g_field_count].id = (uintptr_t)(g_field_count + 5000);
        return g_fields[g_field_count++].id;
    }
    return 1;
}

static FakeMethod* find_method_by_id(uintptr_t id) {
    for (int i = 0; i < g_method_count; i++) {
        if (g_methods[i].id == id) return &g_methods[i];
    }
    return NULL;
}

static FakeArray* as_fake_array(jarray arr) {
    if ((uintptr_t)arr <= 0x1000) return NULL;
    FakeArray *fa = (FakeArray *)arr;
    if (fa->magic != FAKE_OBJ_MAGIC || fa->kind != FAKE_KIND_ARRAY) return NULL;
    return fa;
}

// Generic fallback stub
static jint GetVersion_stub(JNIEnv *env) { return JNI_VERSION_1_6; }
static jclass DefineClass_stub(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize len) { return (jclass)1; }
static jclass FindClass_stub(JNIEnv *env, const char *name) { return (jclass)register_class(name); }
static jmethodID FromReflectedMethod_stub(JNIEnv *env, jobject method) { return (jmethodID)1; }
static jfieldID FromReflectedField_stub(JNIEnv *env, jobject field) { return (jfieldID)1; }
static jobject ToReflectedMethod_stub(JNIEnv *env, jclass cls, jmethodID methodID, jboolean isStatic) { return (jobject)1; }
static jclass GetSuperclass_stub(JNIEnv *env, jclass sub) { return (jclass)1; }
static jboolean IsAssignableFrom_stub(JNIEnv *env, jclass sub, jclass sup) { return JNI_TRUE; }
static jobject ToReflectedField_stub(JNIEnv *env, jclass cls, jfieldID fieldID, jboolean isStatic) { return (jobject)1; }
static jint Throw_stub(JNIEnv *env, jthrowable obj) { return 0; }
static jint ThrowNew_stub(JNIEnv *env, jclass clazz, const char *msg) { return 0; }
static jthrowable ExceptionOccurred_stub(JNIEnv *env) { return NULL; }
static void ExceptionDescribe_stub(JNIEnv *env) {}
static void ExceptionClear_stub(JNIEnv *env) {}
static void FatalError_stub(JNIEnv *env, const char *msg) { fprintf(stderr, "[JNI FatalError] %s\n", msg ? msg : ""); abort(); }
static jint PushLocalFrame_stub(JNIEnv *env, jint capacity) { return 0; }
static jobject PopLocalFrame_stub(JNIEnv *env, jobject result) { return result; }
static jobject NewGlobalRef_stub(JNIEnv *env, jobject obj) { return obj ? obj : (jobject)1; }
static void DeleteGlobalRef_stub(JNIEnv *env, jobject obj) {}
static void DeleteLocalRef_stub(JNIEnv *env, jobject obj) {
    FakeArray *fa = as_fake_array(obj);
    if (fa) {
        free(fa->data);
        fa->magic = 0;
        free(fa);
    }
}
static jboolean IsSameObject_stub(JNIEnv *env, jobject obj1, jobject obj2) { return (obj1 == obj2) ? JNI_TRUE : JNI_FALSE; }
static jobject NewLocalRef_stub(JNIEnv *env, jobject obj) { return obj ? obj : (jobject)1; }
static jint EnsureLocalCapacity_stub(JNIEnv *env, jint capacity) { return 0; }
static jobject AllocObject_stub(JNIEnv *env, jclass clazz) { return (jobject)1; }
static jclass GetObjectClass_stub(JNIEnv *env, jobject obj) { return (jclass)1; }
static jboolean IsInstanceOf_stub(JNIEnv *env, jobject obj, jclass clazz) { return JNI_TRUE; }

static jmethodID GetMethodID_stub(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    return (jmethodID)register_method((uintptr_t)clazz, name, sig);
}
static jmethodID GetStaticMethodID_stub(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    return (jmethodID)register_method((uintptr_t)clazz, name, sig);
}
static jfieldID GetFieldID_stub(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    return (jfieldID)register_field((uintptr_t)clazz, name, sig);
}
static jfieldID GetStaticFieldID_stub(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    return (jfieldID)register_field((uintptr_t)clazz, name, sig);
}

static jobject NewObjectV_stub(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args) { return (jobject)1; }
static jobject NewObjectA_stub(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args) { return (jobject)1; }
static jobject NewObject_stub(JNIEnv *env, jclass clazz, jmethodID methodID, ...) { return (jobject)1; }

static jobject CallObjectMethodV_stub(JNIEnv *env, jobject obj, jmethodID methodID, va_list args) {
    FakeMethod *m = find_method_by_id((uintptr_t)methodID);
    if (m) {
        if (strcmp(m->name, "GetModelName") == 0) {
            return (jobject)strdup("RG35XX Plus");
        } else if (strcmp(m->name, "GetPlatformVersion") == 0) {
            return (jobject)strdup("4.4.2");
        } else if (strcmp(m->name, "GetIMEI") == 0 ||
                   strcmp(m->name, "GetPhoneNr") == 0 ||
                   strcmp(m->name, "GetOperatorName") == 0 ||
                   strcmp(m->name, "GetCountryCode") == 0) {
            return (jobject)strdup("000000000000000");
        } else if (strcmp(m->name, "GetMessage") == 0) {
            return (jobject)strdup("");
        }
    }
    return (jobject)strdup("");
}
static jobject CallObjectMethod_stub(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
    va_list args; va_start(args, methodID);
    jobject res = CallObjectMethodV_stub(env, obj, methodID, args);
    va_end(args); return res;
}
static jobject CallObjectMethodA_stub(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args) { return (jobject)strdup(""); }

static jboolean CallBooleanMethodV_stub(JNIEnv *env, jobject obj, jmethodID methodID, va_list args) {
    FakeMethod *m = find_method_by_id((uintptr_t)methodID);
    if (m) {
        if (strcmp(m->name, "EglUpdateDisplay") == 0) {
            if (g_display_swap_cb) g_display_swap_cb();
            return JNI_TRUE;
        } else if (strcmp(m->name, "EglCreate") == 0) {
            jintArray arr = va_arg(args, jintArray);
            FakeArray *fa = as_fake_array(arr);
            if (fa && fa->data) {
                jint *attribs = (jint *)fa->data;
                printf("[FakeJNI] FuseEgl::EglCreate(attribs[%d]):", (int)fa->length);
                for (int i = 0; i < fa->length; i++) {
                    printf(" 0x%x", (unsigned int)attribs[i]);
                }
                printf("\n");
            }
            return JNI_TRUE;
        }
    }
    return JNI_TRUE;
}
static jboolean CallBooleanMethod_stub(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
    va_list args; va_start(args, methodID);
    jboolean res = CallBooleanMethodV_stub(env, obj, methodID, args);
    va_end(args); return res;
}
static jboolean CallBooleanMethodA_stub(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args) { return JNI_TRUE; }

static jbyte CallByteMethodV_stub(JNIEnv *env, jobject obj, jmethodID m, va_list a) { return 0; }
static jbyte CallByteMethod_stub(JNIEnv *env, jobject obj, jmethodID m, ...) { return 0; }
static jbyte CallByteMethodA_stub(JNIEnv *env, jobject obj, jmethodID m, const jvalue *a) { return 0; }
static jchar CallCharMethodV_stub(JNIEnv *env, jobject obj, jmethodID m, va_list a) { return 0; }
static jchar CallCharMethod_stub(JNIEnv *env, jobject obj, jmethodID m, ...) { return 0; }
static jchar CallCharMethodA_stub(JNIEnv *env, jobject obj, jmethodID m, const jvalue *a) { return 0; }
static jshort CallShortMethodV_stub(JNIEnv *env, jobject obj, jmethodID m, va_list a) { return 0; }
static jshort CallShortMethod_stub(JNIEnv *env, jobject obj, jmethodID m, ...) { return 0; }
static jshort CallShortMethodA_stub(JNIEnv *env, jobject obj, jmethodID m, const jvalue *a) { return 0; }

static jint CallIntMethodV_stub(JNIEnv *env, jobject obj, jmethodID methodID, va_list args) {
    FakeMethod *m = find_method_by_id((uintptr_t)methodID);
    if (m) {
        if (strcmp(m->name, "AudioCreate") == 0) {
            jint freq = va_arg(args, jint);
            jint channels = va_arg(args, jint);
            jint bits = va_arg(args, jint);
            jint buf_size = va_arg(args, jint);
            printf("[FakeJNI] FuseAudio::AudioCreate(freq=%d, ch=%d, bits=%d, buf=%d)\n",
                   (int)freq, (int)channels, (int)bits, (int)buf_size);
            if (g_audio_create_cb) {
                return g_audio_create_cb(freq, channels, bits, buf_size);
            }
            return 1;
        } else if (strcmp(m->name, "AudioStart") == 0) {
            if (g_audio_start_cb) return g_audio_start_cb();
            return 0;
        } else if (strcmp(m->name, "AudioGetVolume") == 0) {
            return 100;
        } else if (strcmp(m->name, "GetStatus") == 0 || strcmp(m->name, "Show") == 0) {
            return 0;
        } else if (strcmp(m->name, "doLicenseCheck") == 0) {
            return 1;
        }
    }
    return 0;
}
static jint CallIntMethod_stub(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
    va_list args; va_start(args, methodID);
    jint res = CallIntMethodV_stub(env, obj, methodID, args);
    va_end(args); return res;
}
static jint CallIntMethodA_stub(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args) { return 0; }

static jlong CallLongMethodV_stub(JNIEnv *env, jobject obj, jmethodID m, va_list a) {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (jlong)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}
static jlong CallLongMethod_stub(JNIEnv *env, jobject obj, jmethodID m, ...) {
    struct timeval tv; gettimeofday(&tv, NULL);
    return (jlong)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}
static jlong CallLongMethodA_stub(JNIEnv *env, jobject obj, jmethodID m, const jvalue *a) { return 0; }
static jfloat CallFloatMethodV_stub(JNIEnv *env, jobject obj, jmethodID m, va_list a) { return 0.0f; }
static jfloat CallFloatMethod_stub(JNIEnv *env, jobject obj, jmethodID m, ...) { return 0.0f; }
static jfloat CallFloatMethodA_stub(JNIEnv *env, jobject obj, jmethodID m, const jvalue *a) { return 0.0f; }
static jdouble CallDoubleMethodV_stub(JNIEnv *env, jobject obj, jmethodID m, va_list a) { return 0.0; }
static jdouble CallDoubleMethod_stub(JNIEnv *env, jobject obj, jmethodID m, ...) { return 0.0; }
static jdouble CallDoubleMethodA_stub(JNIEnv *env, jobject obj, jmethodID m, const jvalue *a) { return 0.0; }

static void CallVoidMethodV_stub(JNIEnv *env, jobject obj, jmethodID methodID, va_list args) {
    FakeMethod *m = find_method_by_id((uintptr_t)methodID);
    if (m && strcmp(m->name, "AudioStop") == 0) {
        if (g_audio_stop_cb) g_audio_stop_cb();
    }
}
static void CallVoidMethod_stub(JNIEnv *env, jobject obj, jmethodID methodID, ...) {
    va_list args; va_start(args, methodID);
    CallVoidMethodV_stub(env, obj, methodID, args);
    va_end(args);
}
static void CallVoidMethodA_stub(JNIEnv *env, jobject obj, jmethodID methodID, const jvalue *args) {}

// Nonvirtual stubs
static jobject CallNonvirtualObjectMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return (jobject)strdup(""); }
static jobject CallNonvirtualObjectMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return (jobject)strdup(""); }
static jobject CallNonvirtualObjectMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return (jobject)strdup(""); }
static jboolean CallNonvirtualBooleanMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return JNI_TRUE; }
static jboolean CallNonvirtualBooleanMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return JNI_TRUE; }
static jboolean CallNonvirtualBooleanMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return JNI_TRUE; }
static jbyte CallNonvirtualByteMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return 0; }
static jbyte CallNonvirtualByteMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return 0; }
static jbyte CallNonvirtualByteMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jchar CallNonvirtualCharMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return 0; }
static jchar CallNonvirtualCharMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return 0; }
static jchar CallNonvirtualCharMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jshort CallNonvirtualShortMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return 0; }
static jshort CallNonvirtualShortMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return 0; }
static jshort CallNonvirtualShortMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jint CallNonvirtualIntMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return 0; }
static jint CallNonvirtualIntMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return 0; }
static jint CallNonvirtualIntMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jlong CallNonvirtualLongMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return 0; }
static jlong CallNonvirtualLongMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return 0; }
static jlong CallNonvirtualLongMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jfloat CallNonvirtualFloatMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return 0.0f; }
static jfloat CallNonvirtualFloatMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return 0.0f; }
static jfloat CallNonvirtualFloatMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return 0.0f; }
static jdouble CallNonvirtualDoubleMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) { return 0.0; }
static jdouble CallNonvirtualDoubleMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) { return 0.0; }
static jdouble CallNonvirtualDoubleMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) { return 0.0; }
static void CallNonvirtualVoidMethod_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, ...) {}
static void CallNonvirtualVoidMethodV_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, va_list a) {}
static void CallNonvirtualVoidMethodA_stub(JNIEnv *e, jobject o, jclass c, jmethodID m, const jvalue *a) {}

// Field stubs
static jobject GetObjectField_stub(JNIEnv *e, jobject o, jfieldID f) { return (jobject)1; }
static jboolean GetBooleanField_stub(JNIEnv *e, jobject o, jfieldID f) { return JNI_FALSE; }
static jbyte GetByteField_stub(JNIEnv *e, jobject o, jfieldID f) { return 0; }
static jchar GetCharField_stub(JNIEnv *e, jobject o, jfieldID f) { return 0; }
static jshort GetShortField_stub(JNIEnv *e, jobject o, jfieldID f) { return 0; }
static jint GetIntField_stub(JNIEnv *e, jobject o, jfieldID f) { return 0; }
static jlong GetLongField_stub(JNIEnv *e, jobject o, jfieldID f) { return 0; }
static jfloat GetFloatField_stub(JNIEnv *e, jobject o, jfieldID f) { return 0.0f; }
static jdouble GetDoubleField_stub(JNIEnv *e, jobject o, jfieldID f) { return 0.0; }
static void SetObjectField_stub(JNIEnv *e, jobject o, jfieldID f, jobject v) {}
static void SetBooleanField_stub(JNIEnv *e, jobject o, jfieldID f, jboolean v) {}
static void SetByteField_stub(JNIEnv *e, jobject o, jfieldID f, jbyte v) {}
static void SetCharField_stub(JNIEnv *e, jobject o, jfieldID f, jchar v) {}
static void SetShortField_stub(JNIEnv *e, jobject o, jfieldID f, jshort v) {}
static void SetIntField_stub(JNIEnv *e, jobject o, jfieldID f, jint v) {}
static void SetLongField_stub(JNIEnv *e, jobject o, jfieldID f, jlong v) {}
static void SetFloatField_stub(JNIEnv *e, jobject o, jfieldID f, jfloat v) {}
static void SetDoubleField_stub(JNIEnv *e, jobject o, jfieldID f, jdouble v) {}

// Static method calls
static jobject CallStaticObjectMethod_stub(JNIEnv *env, jclass clazz, jmethodID methodID, ...) { return (jobject)strdup(""); }
static jobject CallStaticObjectMethodV_stub(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args) { return (jobject)strdup(""); }
static jobject CallStaticObjectMethodA_stub(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args) { return (jobject)strdup(""); }
static jboolean CallStaticBooleanMethod_stub(JNIEnv *env, jclass clazz, jmethodID methodID, ...) { return JNI_TRUE; }
static jboolean CallStaticBooleanMethodV_stub(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args) { return JNI_TRUE; }
static jboolean CallStaticBooleanMethodA_stub(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args) { return JNI_TRUE; }
static jbyte CallStaticByteMethod_stub(JNIEnv *e, jclass c, jmethodID m, ...) { return 0; }
static jbyte CallStaticByteMethodV_stub(JNIEnv *e, jclass c, jmethodID m, va_list a) { return 0; }
static jbyte CallStaticByteMethodA_stub(JNIEnv *e, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jchar CallStaticCharMethod_stub(JNIEnv *e, jclass c, jmethodID m, ...) { return 0; }
static jchar CallStaticCharMethodV_stub(JNIEnv *e, jclass c, jmethodID m, va_list a) { return 0; }
static jchar CallStaticCharMethodA_stub(JNIEnv *e, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jshort CallStaticShortMethod_stub(JNIEnv *e, jclass c, jmethodID m, ...) { return 0; }
static jshort CallStaticShortMethodV_stub(JNIEnv *e, jclass c, jmethodID m, va_list a) { return 0; }
static jshort CallStaticShortMethodA_stub(JNIEnv *e, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jint CallStaticIntMethod_stub(JNIEnv *env, jclass clazz, jmethodID methodID, ...) { return 0; }
static jint CallStaticIntMethodV_stub(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args) { return 0; }
static jint CallStaticIntMethodA_stub(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args) { return 0; }
static jlong CallStaticLongMethod_stub(JNIEnv *e, jclass c, jmethodID m, ...) { return 0; }
static jlong CallStaticLongMethodV_stub(JNIEnv *e, jclass c, jmethodID m, va_list a) { return 0; }
static jlong CallStaticLongMethodA_stub(JNIEnv *e, jclass c, jmethodID m, const jvalue *a) { return 0; }
static jfloat CallStaticFloatMethod_stub(JNIEnv *e, jclass c, jmethodID m, ...) { return 0.0f; }
static jfloat CallStaticFloatMethodV_stub(JNIEnv *e, jclass c, jmethodID m, va_list a) { return 0.0f; }
static jfloat CallStaticFloatMethodA_stub(JNIEnv *e, jclass c, jmethodID m, const jvalue *a) { return 0.0f; }
static jdouble CallStaticDoubleMethod_stub(JNIEnv *e, jclass c, jmethodID m, ...) { return 0.0; }
static jdouble CallStaticDoubleMethodV_stub(JNIEnv *e, jclass c, jmethodID m, va_list a) { return 0.0; }
static jdouble CallStaticDoubleMethodA_stub(JNIEnv *e, jclass c, jmethodID m, const jvalue *a) { return 0.0; }
static void CallStaticVoidMethod_stub(JNIEnv *env, jclass clazz, jmethodID methodID, ...) {}
static void CallStaticVoidMethodV_stub(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args) {}
static void CallStaticVoidMethodA_stub(JNIEnv *env, jclass clazz, jmethodID methodID, const jvalue *args) {}

// Static field stubs
static jobject GetStaticObjectField_stub(JNIEnv *e, jclass c, jfieldID f) { return (jobject)1; }
static jboolean GetStaticBooleanField_stub(JNIEnv *e, jclass c, jfieldID f) { return JNI_FALSE; }
static jbyte GetStaticByteField_stub(JNIEnv *e, jclass c, jfieldID f) { return 0; }
static jchar GetStaticCharField_stub(JNIEnv *e, jclass c, jfieldID f) { return 0; }
static jshort GetStaticShortField_stub(JNIEnv *e, jclass c, jfieldID f) { return 0; }
static jint GetStaticIntField_stub(JNIEnv *e, jclass c, jfieldID f) { return 0; }
static jlong GetStaticLongField_stub(JNIEnv *e, jclass c, jfieldID f) { return 0; }
static jfloat GetStaticFloatField_stub(JNIEnv *e, jclass c, jfieldID f) { return 0.0f; }
static jdouble GetStaticDoubleField_stub(JNIEnv *e, jclass c, jfieldID f) { return 0.0; }
static void SetStaticObjectField_stub(JNIEnv *e, jclass c, jfieldID f, jobject v) {}
static void SetStaticBooleanField_stub(JNIEnv *e, jclass c, jfieldID f, jboolean v) {}
static void SetStaticByteField_stub(JNIEnv *e, jclass c, jfieldID f, jbyte v) {}
static void SetStaticCharField_stub(JNIEnv *e, jclass c, jfieldID f, jchar v) {}
static void SetStaticShortField_stub(JNIEnv *e, jclass c, jfieldID f, jshort v) {}
static void SetStaticIntField_stub(JNIEnv *e, jclass c, jfieldID f, jint v) {}
static void SetStaticLongField_stub(JNIEnv *e, jclass c, jfieldID f, jlong v) {}
static void SetStaticFloatField_stub(JNIEnv *e, jclass c, jfieldID f, jfloat v) {}
static void SetStaticDoubleField_stub(JNIEnv *e, jclass c, jfieldID f, jdouble v) {}

// String functions
static jstring NewString_stub(JNIEnv *env, const jchar *chars, jsize len) { return (jstring)strdup(""); }
static jsize GetStringLength_stub(JNIEnv *env, jstring str) { return 0; }
static const jchar* GetStringChars_stub(JNIEnv *env, jstring str, jboolean *isCopy) {
    static const jchar empty = 0;
    if (isCopy) *isCopy = JNI_FALSE;
    return &empty;
}
static void ReleaseStringChars_stub(JNIEnv *env, jstring str, const jchar *chars) {}

static jstring NewStringUTF_stub(JNIEnv *env, const char *bytes) {
    if (!bytes) return NULL;
    return (jstring)strdup(bytes);
}

static const char* GetStringUTFChars_stub(JNIEnv *env, jstring string, jboolean *isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if ((uintptr_t)string <= 0x1000) return "";
    return (const char *)string;
}

static void ReleaseStringUTFChars_stub(JNIEnv *env, jstring string, const char *utf) {}

static jsize GetStringUTFLength_stub(JNIEnv *env, jstring string) {
    if ((uintptr_t)string <= 0x1000) return 0;
    return (jsize)strlen((const char *)string);
}

// Array allocation & accessors
static jarray alloc_fake_array(jsize length, size_t elem_size) {
    if (length < 0) length = 0;
    FakeArray *fa = (FakeArray *)malloc(sizeof(FakeArray));
    if (!fa) return NULL;
    fa->magic = FAKE_OBJ_MAGIC;
    fa->kind = FAKE_KIND_ARRAY;
    fa->length = length;
    fa->elem_size = elem_size;
    fa->data = calloc((size_t)(length > 0 ? length : 1), elem_size);
    return (jarray)fa;
}

static jsize GetArrayLength_stub(JNIEnv *env, jarray array) {
    FakeArray *fa = as_fake_array(array);
    return fa ? fa->length : 0;
}

static jobjectArray NewObjectArray_stub(JNIEnv *env, jsize len, jclass cls, jobject init) {
    jobjectArray arr = (jobjectArray)alloc_fake_array(len, sizeof(jobject));
    FakeArray *fa = as_fake_array(arr);
    if (fa && fa->data && init) {
        jobject *elems = (jobject *)fa->data;
        for (jsize i = 0; i < len; i++) elems[i] = init;
    }
    return arr;
}
static jobject GetObjectArrayElement_stub(JNIEnv *env, jobjectArray array, jsize index) {
    FakeArray *fa = as_fake_array(array);
    if (!fa || index < 0 || index >= fa->length) return NULL;
    return ((jobject *)fa->data)[index];
}
static void SetObjectArrayElement_stub(JNIEnv *env, jobjectArray array, jsize index, jobject val) {
    FakeArray *fa = as_fake_array(array);
    if (fa && index >= 0 && index < fa->length) ((jobject *)fa->data)[index] = val;
}

static jbooleanArray NewBooleanArray_stub(JNIEnv *env, jsize len) { return (jbooleanArray)alloc_fake_array(len, sizeof(jboolean)); }
static jbyteArray    NewByteArray_stub(JNIEnv *env, jsize len)    { return (jbyteArray)alloc_fake_array(len, sizeof(jbyte)); }
static jcharArray    NewCharArray_stub(JNIEnv *env, jsize len)    { return (jcharArray)alloc_fake_array(len, sizeof(jchar)); }
static jshortArray   NewShortArray_stub(JNIEnv *env, jsize len)   { return (jshortArray)alloc_fake_array(len, sizeof(jshort)); }
static jintArray     NewIntArray_stub(JNIEnv *env, jsize len)     { return (jintArray)alloc_fake_array(len, sizeof(jint)); }
static jlongArray    NewLongArray_stub(JNIEnv *env, jsize len)    { return (jlongArray)alloc_fake_array(len, sizeof(jlong)); }
static jfloatArray   NewFloatArray_stub(JNIEnv *env, jsize len)   { return (jfloatArray)alloc_fake_array(len, sizeof(jfloat)); }
static jdoubleArray  NewDoubleArray_stub(JNIEnv *env, jsize len)  { return (jdoubleArray)alloc_fake_array(len, sizeof(jdouble)); }

#define IMPL_PRIM_ARRAY(JType, Name) \
static JType* Get##Name##ArrayElements_stub(JNIEnv *env, JType##Array array, jboolean *isCopy) { \
    if (isCopy) *isCopy = JNI_FALSE; \
    FakeArray *fa = as_fake_array(array); \
    return fa ? (JType *)fa->data : NULL; \
} \
static void Release##Name##ArrayElements_stub(JNIEnv *env, JType##Array array, JType *elems, jint mode) {} \
static void Get##Name##ArrayRegion_stub(JNIEnv *env, JType##Array array, jsize start, jsize len, JType *buf) { \
    FakeArray *fa = as_fake_array(array); \
    if (fa && fa->data && buf && start >= 0 && len > 0 && start + len <= fa->length) { \
        memcpy(buf, (JType *)fa->data + start, (size_t)len * sizeof(JType)); \
    } \
} \
static void Set##Name##ArrayRegion_stub(JNIEnv *env, JType##Array array, jsize start, jsize len, const JType *buf) { \
    FakeArray *fa = as_fake_array(array); \
    if (fa && fa->data && buf && start >= 0 && len > 0 && start + len <= fa->length) { \
        memcpy((JType *)fa->data + start, buf, (size_t)len * sizeof(JType)); \
    } \
}

IMPL_PRIM_ARRAY(jboolean, Boolean)
IMPL_PRIM_ARRAY(jbyte, Byte)
IMPL_PRIM_ARRAY(jchar, Char)
IMPL_PRIM_ARRAY(jshort, Short)
IMPL_PRIM_ARRAY(jint, Int)
IMPL_PRIM_ARRAY(jlong, Long)
IMPL_PRIM_ARRAY(jfloat, Float)
IMPL_PRIM_ARRAY(jdouble, Double)

static jint RegisterNatives_stub(JNIEnv *env, jclass cls, const void *methods, jint nMethods) { return 0; }
static jint UnregisterNatives_stub(JNIEnv *env, jclass cls) { return 0; }
static jint MonitorEnter_stub(JNIEnv *env, jobject obj) { return 0; }
static jint MonitorExit_stub(JNIEnv *env, jobject obj) { return 0; }
static jint GetJavaVM_stub(JNIEnv *env, JavaVM **vm) { if (vm) *vm = get_fake_java_vm(); return JNI_OK; }
static void GetStringRegion_stub(JNIEnv *env, jstring str, jsize start, jsize len, jchar *buf) {}
static void GetStringUTFRegion_stub(JNIEnv *env, jstring str, jsize start, jsize len, char *buf) {
    const char *s = GetStringUTFChars_stub(env, str, NULL);
    if (buf && len > 0) {
        size_t slen = strlen(s);
        if ((size_t)start < slen) strncpy(buf, s + start, (size_t)len);
    }
}
static void* GetPrimitiveArrayCritical_stub(JNIEnv *env, jarray array, jboolean *isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    FakeArray *fa = as_fake_array(array);
    return fa ? fa->data : NULL;
}
static void ReleasePrimitiveArrayCritical_stub(JNIEnv *env, jarray array, void *carray, jint mode) {}
static const jchar* GetStringCritical_stub(JNIEnv *env, jstring str, jboolean *isCopy) { return GetStringChars_stub(env, str, isCopy); }
static void ReleaseStringCritical_stub(JNIEnv *env, jstring str, const jchar *carray) {}
static jboolean ExceptionCheck_stub(JNIEnv *env) { return JNI_FALSE; }

// Direct Buffer Operations (CRITICAL FOR AUDIOMIX)
static jobject NewDirectByteBuffer_stub(JNIEnv *env, void *address, jlong capacity) {
    for (int i = 0; i < MAX_DIRECT_BUFFERS; i++) {
        if (!g_direct_buffers[i].in_use) {
            g_direct_buffers[i].magic = FAKE_OBJ_MAGIC;
            g_direct_buffers[i].kind = FAKE_KIND_BUFFER;
            g_direct_buffers[i].address = address;
            g_direct_buffers[i].capacity = capacity;
            g_direct_buffers[i].in_use = 1;
            return (jobject)&g_direct_buffers[i];
        }
    }
    return NULL;
}

static void* GetDirectBufferAddress_stub(JNIEnv *env, jobject buf) {
    if ((uintptr_t)buf <= 0x1000) return NULL;
    DirectBuffer *db = (DirectBuffer *)buf;
    if (db->magic != FAKE_OBJ_MAGIC || db->kind != FAKE_KIND_BUFFER) return NULL;
    return db->address;
}

static jlong GetDirectBufferCapacity_stub(JNIEnv *env, jobject buf) {
    if ((uintptr_t)buf <= 0x1000) return 0;
    DirectBuffer *db = (DirectBuffer *)buf;
    if (db->magic != FAKE_OBJ_MAGIC || db->kind != FAKE_KIND_BUFFER) return 0;
    return db->capacity;
}

jobject fake_jni_create_direct_buffer(void *addr, jlong capacity) {
    return NewDirectByteBuffer_stub(get_fake_jni_env(), addr, capacity);
}

static struct JNINativeInterface_ g_jni_native_interface = {
    .GetVersion = GetVersion_stub,
    .DefineClass = DefineClass_stub,
    .FindClass = FindClass_stub,
    .FromReflectedMethod = FromReflectedMethod_stub,
    .FromReflectedField = FromReflectedField_stub,
    .ToReflectedMethod = ToReflectedMethod_stub,
    .GetSuperclass = GetSuperclass_stub,
    .IsAssignableFrom = IsAssignableFrom_stub,
    .ToReflectedField = ToReflectedField_stub,
    .Throw = Throw_stub,
    .ThrowNew = ThrowNew_stub,
    .ExceptionOccurred = ExceptionOccurred_stub,
    .ExceptionDescribe = ExceptionDescribe_stub,
    .ExceptionClear = ExceptionClear_stub,
    .FatalError = FatalError_stub,
    .PushLocalFrame = PushLocalFrame_stub,
    .PopLocalFrame = PopLocalFrame_stub,
    .NewGlobalRef = NewGlobalRef_stub,
    .DeleteGlobalRef = DeleteGlobalRef_stub,
    .DeleteLocalRef = DeleteLocalRef_stub,
    .IsSameObject = IsSameObject_stub,
    .NewLocalRef = NewLocalRef_stub,
    .EnsureLocalCapacity = EnsureLocalCapacity_stub,
    .AllocObject = AllocObject_stub,
    .NewObject = NewObject_stub,
    .NewObjectV = NewObjectV_stub,
    .NewObjectA = NewObjectA_stub,
    .GetObjectClass = GetObjectClass_stub,
    .IsInstanceOf = IsInstanceOf_stub,
    .GetMethodID = GetMethodID_stub,
    .CallObjectMethod = CallObjectMethod_stub,
    .CallObjectMethodV = CallObjectMethodV_stub,
    .CallObjectMethodA = CallObjectMethodA_stub,
    .CallBooleanMethod = CallBooleanMethod_stub,
    .CallBooleanMethodV = CallBooleanMethodV_stub,
    .CallBooleanMethodA = CallBooleanMethodA_stub,
    .CallByteMethod = CallByteMethod_stub,
    .CallByteMethodV = CallByteMethodV_stub,
    .CallByteMethodA = CallByteMethodA_stub,
    .CallCharMethod = CallCharMethod_stub,
    .CallCharMethodV = CallCharMethodV_stub,
    .CallCharMethodA = CallCharMethodA_stub,
    .CallShortMethod = CallShortMethod_stub,
    .CallShortMethodV = CallShortMethodV_stub,
    .CallShortMethodA = CallShortMethodA_stub,
    .CallIntMethod = CallIntMethod_stub,
    .CallIntMethodV = CallIntMethodV_stub,
    .CallIntMethodA = CallIntMethodA_stub,
    .CallLongMethod = CallLongMethod_stub,
    .CallLongMethodV = CallLongMethodV_stub,
    .CallLongMethodA = CallLongMethodA_stub,
    .CallFloatMethod = CallFloatMethod_stub,
    .CallFloatMethodV = CallFloatMethodV_stub,
    .CallFloatMethodA = CallFloatMethodA_stub,
    .CallDoubleMethod = CallDoubleMethod_stub,
    .CallDoubleMethodV = CallDoubleMethodV_stub,
    .CallDoubleMethodA = CallDoubleMethodA_stub,
    .CallVoidMethod = CallVoidMethod_stub,
    .CallVoidMethodV = CallVoidMethodV_stub,
    .CallVoidMethodA = CallVoidMethodA_stub,
    .CallNonvirtualObjectMethod = CallNonvirtualObjectMethod_stub,
    .CallNonvirtualObjectMethodV = CallNonvirtualObjectMethodV_stub,
    .CallNonvirtualObjectMethodA = CallNonvirtualObjectMethodA_stub,
    .CallNonvirtualBooleanMethod = CallNonvirtualBooleanMethod_stub,
    .CallNonvirtualBooleanMethodV = CallNonvirtualBooleanMethodV_stub,
    .CallNonvirtualBooleanMethodA = CallNonvirtualBooleanMethodA_stub,
    .CallNonvirtualByteMethod = CallNonvirtualByteMethod_stub,
    .CallNonvirtualByteMethodV = CallNonvirtualByteMethodV_stub,
    .CallNonvirtualByteMethodA = CallNonvirtualByteMethodA_stub,
    .CallNonvirtualCharMethod = CallNonvirtualCharMethod_stub,
    .CallNonvirtualCharMethodV = CallNonvirtualCharMethodV_stub,
    .CallNonvirtualCharMethodA = CallNonvirtualCharMethodA_stub,
    .CallNonvirtualShortMethod = CallNonvirtualShortMethod_stub,
    .CallNonvirtualShortMethodV = CallNonvirtualShortMethodV_stub,
    .CallNonvirtualShortMethodA = CallNonvirtualShortMethodA_stub,
    .CallNonvirtualIntMethod = CallNonvirtualIntMethod_stub,
    .CallNonvirtualIntMethodV = CallNonvirtualIntMethodV_stub,
    .CallNonvirtualIntMethodA = CallNonvirtualIntMethodA_stub,
    .CallNonvirtualLongMethod = CallNonvirtualLongMethod_stub,
    .CallNonvirtualLongMethodV = CallNonvirtualLongMethodV_stub,
    .CallNonvirtualLongMethodA = CallNonvirtualLongMethodA_stub,
    .CallNonvirtualFloatMethod = CallNonvirtualFloatMethod_stub,
    .CallNonvirtualFloatMethodV = CallNonvirtualFloatMethodV_stub,
    .CallNonvirtualFloatMethodA = CallNonvirtualFloatMethodA_stub,
    .CallNonvirtualDoubleMethod = CallNonvirtualDoubleMethod_stub,
    .CallNonvirtualDoubleMethodV = CallNonvirtualDoubleMethodV_stub,
    .CallNonvirtualDoubleMethodA = CallNonvirtualDoubleMethodA_stub,
    .CallNonvirtualVoidMethod = CallNonvirtualVoidMethod_stub,
    .CallNonvirtualVoidMethodV = CallNonvirtualVoidMethodV_stub,
    .CallNonvirtualVoidMethodA = CallNonvirtualVoidMethodA_stub,
    .GetFieldID = GetFieldID_stub,
    .GetObjectField = GetObjectField_stub,
    .GetBooleanField = GetBooleanField_stub,
    .GetByteField = GetByteField_stub,
    .GetCharField = GetCharField_stub,
    .GetShortField = GetShortField_stub,
    .GetIntField = GetIntField_stub,
    .GetLongField = GetLongField_stub,
    .GetFloatField = GetFloatField_stub,
    .GetDoubleField = GetDoubleField_stub,
    .SetObjectField = SetObjectField_stub,
    .SetBooleanField = SetBooleanField_stub,
    .SetByteField = SetByteField_stub,
    .SetCharField = SetCharField_stub,
    .SetShortField = SetShortField_stub,
    .SetIntField = SetIntField_stub,
    .SetLongField = SetLongField_stub,
    .SetFloatField = SetFloatField_stub,
    .SetDoubleField = SetDoubleField_stub,
    .GetStaticMethodID = GetStaticMethodID_stub,
    .CallStaticObjectMethod = CallStaticObjectMethod_stub,
    .CallStaticObjectMethodV = CallStaticObjectMethodV_stub,
    .CallStaticObjectMethodA = CallStaticObjectMethodA_stub,
    .CallStaticBooleanMethod = CallStaticBooleanMethod_stub,
    .CallStaticBooleanMethodV = CallStaticBooleanMethodV_stub,
    .CallStaticBooleanMethodA = CallStaticBooleanMethodA_stub,
    .CallStaticByteMethod = CallStaticByteMethod_stub,
    .CallStaticByteMethodV = CallStaticByteMethodV_stub,
    .CallStaticByteMethodA = CallStaticByteMethodA_stub,
    .CallStaticCharMethod = CallStaticCharMethod_stub,
    .CallStaticCharMethodV = CallStaticCharMethodV_stub,
    .CallStaticCharMethodA = CallStaticCharMethodA_stub,
    .CallStaticShortMethod = CallStaticShortMethod_stub,
    .CallStaticShortMethodV = CallStaticShortMethodV_stub,
    .CallStaticShortMethodA = CallStaticShortMethodA_stub,
    .CallStaticIntMethod = CallStaticIntMethod_stub,
    .CallStaticIntMethodV = CallStaticIntMethodV_stub,
    .CallStaticIntMethodA = CallStaticIntMethodA_stub,
    .CallStaticLongMethod = CallStaticLongMethod_stub,
    .CallStaticLongMethodV = CallStaticLongMethodV_stub,
    .CallStaticLongMethodA = CallStaticLongMethodA_stub,
    .CallStaticFloatMethod = CallStaticFloatMethod_stub,
    .CallStaticFloatMethodV = CallStaticFloatMethodV_stub,
    .CallStaticFloatMethodA = CallStaticFloatMethodA_stub,
    .CallStaticDoubleMethod = CallStaticDoubleMethod_stub,
    .CallStaticDoubleMethodV = CallStaticDoubleMethodV_stub,
    .CallStaticDoubleMethodA = CallStaticDoubleMethodA_stub,
    .CallStaticVoidMethod = CallStaticVoidMethod_stub,
    .CallStaticVoidMethodV = CallStaticVoidMethodV_stub,
    .CallStaticVoidMethodA = CallStaticVoidMethodA_stub,
    .GetStaticFieldID = GetStaticFieldID_stub,
    .GetStaticObjectField = GetStaticObjectField_stub,
    .GetStaticBooleanField = GetStaticBooleanField_stub,
    .GetStaticByteField = GetStaticByteField_stub,
    .GetStaticCharField = GetStaticCharField_stub,
    .GetStaticShortField = GetStaticShortField_stub,
    .GetStaticIntField = GetStaticIntField_stub,
    .GetStaticLongField = GetStaticLongField_stub,
    .GetStaticFloatField = GetStaticFloatField_stub,
    .GetStaticDoubleField = GetStaticDoubleField_stub,
    .SetStaticObjectField = SetStaticObjectField_stub,
    .SetStaticBooleanField = SetStaticBooleanField_stub,
    .SetStaticByteField = SetStaticByteField_stub,
    .SetStaticCharField = SetStaticCharField_stub,
    .SetStaticShortField = SetStaticShortField_stub,
    .SetStaticIntField = SetStaticIntField_stub,
    .SetStaticLongField = SetStaticLongField_stub,
    .SetStaticFloatField = SetStaticFloatField_stub,
    .SetStaticDoubleField = SetStaticDoubleField_stub,
    .NewString = NewString_stub,
    .GetStringLength = GetStringLength_stub,
    .GetStringChars = GetStringChars_stub,
    .ReleaseStringChars = ReleaseStringChars_stub,
    .NewStringUTF = NewStringUTF_stub,
    .GetStringUTFLength = GetStringUTFLength_stub,
    .GetStringUTFChars = GetStringUTFChars_stub,
    .ReleaseStringUTFChars = ReleaseStringUTFChars_stub,
    .GetArrayLength = GetArrayLength_stub,
    .NewObjectArray = NewObjectArray_stub,
    .GetObjectArrayElement = GetObjectArrayElement_stub,
    .SetObjectArrayElement = SetObjectArrayElement_stub,
    .NewBooleanArray = NewBooleanArray_stub,
    .NewByteArray = NewByteArray_stub,
    .NewCharArray = NewCharArray_stub,
    .NewShortArray = NewShortArray_stub,
    .NewIntArray = NewIntArray_stub,
    .NewLongArray = NewLongArray_stub,
    .NewFloatArray = NewFloatArray_stub,
    .NewDoubleArray = NewDoubleArray_stub,
    .GetBooleanArrayElements = GetBooleanArrayElements_stub,
    .GetByteArrayElements = GetByteArrayElements_stub,
    .GetCharArrayElements = GetCharArrayElements_stub,
    .GetShortArrayElements = GetShortArrayElements_stub,
    .GetIntArrayElements = GetIntArrayElements_stub,
    .GetLongArrayElements = GetLongArrayElements_stub,
    .GetFloatArrayElements = GetFloatArrayElements_stub,
    .GetDoubleArrayElements = GetDoubleArrayElements_stub,
    .ReleaseBooleanArrayElements = ReleaseBooleanArrayElements_stub,
    .ReleaseByteArrayElements = ReleaseByteArrayElements_stub,
    .ReleaseCharArrayElements = ReleaseCharArrayElements_stub,
    .ReleaseShortArrayElements = ReleaseShortArrayElements_stub,
    .ReleaseIntArrayElements = ReleaseIntArrayElements_stub,
    .ReleaseLongArrayElements = ReleaseLongArrayElements_stub,
    .ReleaseFloatArrayElements = ReleaseFloatArrayElements_stub,
    .ReleaseDoubleArrayElements = ReleaseDoubleArrayElements_stub,
    .GetBooleanArrayRegion = GetBooleanArrayRegion_stub,
    .GetByteArrayRegion = GetByteArrayRegion_stub,
    .GetCharArrayRegion = GetCharArrayRegion_stub,
    .GetShortArrayRegion = GetShortArrayRegion_stub,
    .GetIntArrayRegion = GetIntArrayRegion_stub,
    .GetLongArrayRegion = GetLongArrayRegion_stub,
    .GetFloatArrayRegion = GetFloatArrayRegion_stub,
    .GetDoubleArrayRegion = GetDoubleArrayRegion_stub,
    .SetBooleanArrayRegion = SetBooleanArrayRegion_stub,
    .SetByteArrayRegion = SetByteArrayRegion_stub,
    .SetCharArrayRegion = SetCharArrayRegion_stub,
    .SetShortArrayRegion = SetShortArrayRegion_stub,
    .SetIntArrayRegion = SetIntArrayRegion_stub,
    .SetLongArrayRegion = SetLongArrayRegion_stub,
    .SetFloatArrayRegion = SetFloatArrayRegion_stub,
    .SetDoubleArrayRegion = SetDoubleArrayRegion_stub,
    .RegisterNatives = RegisterNatives_stub,
    .UnregisterNatives = UnregisterNatives_stub,
    .MonitorEnter = MonitorEnter_stub,
    .MonitorExit = MonitorExit_stub,
    .GetJavaVM = GetJavaVM_stub,
    .GetStringRegion = GetStringRegion_stub,
    .GetStringUTFRegion = GetStringUTFRegion_stub,
    .GetPrimitiveArrayCritical = GetPrimitiveArrayCritical_stub,
    .ReleasePrimitiveArrayCritical = ReleasePrimitiveArrayCritical_stub,
    .GetStringCritical = GetStringCritical_stub,
    .ReleaseStringCritical = ReleaseStringCritical_stub,
    .NewWeakGlobalRef = NewGlobalRef_stub,
    .DeleteWeakGlobalRef = DeleteGlobalRef_stub,
    .ExceptionCheck = ExceptionCheck_stub,
    .NewDirectByteBuffer = NewDirectByteBuffer_stub,
    .GetDirectBufferAddress = GetDirectBufferAddress_stub,
    .GetDirectBufferCapacity = GetDirectBufferCapacity_stub
};

static const struct JNINativeInterface_ *g_jni_env_ptr = &g_jni_native_interface;

static jint DestroyJavaVM_stub(JavaVM *vm) { return JNI_OK; }
static jint GetEnv_stub(JavaVM *vm, void **env, jint version) {
    if (env) *env = (void *)&g_jni_env_ptr;
    return JNI_OK;
}
static jint AttachCurrentThread_stub(JavaVM *vm, JNIEnv **p_env, void *thr_args) {
    if (p_env) *p_env = (JNIEnv *)&g_jni_env_ptr;
    return JNI_OK;
}
static jint DetachCurrentThread_stub(JavaVM *vm) { return JNI_OK; }

static struct JNIInvokeInterface_ g_jni_invoke_interface = {
    .DestroyJavaVM = DestroyJavaVM_stub,
    .AttachCurrentThread = AttachCurrentThread_stub,
    .DetachCurrentThread = DetachCurrentThread_stub,
    .GetEnv = GetEnv_stub,
    .AttachCurrentThreadAsDaemon = AttachCurrentThread_stub
};

static const struct JNIInvokeInterface_ *g_java_vm_ptr = &g_jni_invoke_interface;

JNIEnv *get_fake_jni_env(void) {
    return (JNIEnv *)&g_jni_env_ptr;
}

JavaVM *get_fake_java_vm(void) {
    return (JavaVM *)&g_java_vm_ptr;
}

void fake_jni_init(const char *apk_path, const char *data_path) {
    if (apk_path) {
        strncpy(g_apk_path, apk_path, sizeof(g_apk_path) - 1);
        g_apk_path[sizeof(g_apk_path) - 1] = '\0';
    }
    if (data_path) {
        strncpy(g_data_path, data_path, sizeof(g_data_path) - 1);
        g_data_path[sizeof(g_data_path) - 1] = '\0';
    }
    memset(g_direct_buffers, 0, sizeof(g_direct_buffers));
}
