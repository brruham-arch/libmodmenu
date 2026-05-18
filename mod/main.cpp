#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <jni.h>
#include "mod/amlmod.h"

MYMOD(brruham.modmenu, ModMenu, 1.0, brruham)

#define LOGFILE "/storage/emulated/0/modmenu_log.txt"
static void logf(const char* fmt, ...) {
    char buf[512]; va_list ap;
    va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    FILE* f = fopen(LOGFILE, "a");
    if (f) { fprintf(f, "%s\n", buf); fclose(f); }
}

static JavaVM* g_jvm    = nullptr;
static int     g_status = 0;

// Native callbacks
static void on_button  (JNIEnv*, jobject, jint id)             { aml->ShowToast(false, "Button %d", (int)id); }
static void on_toggle  (JNIEnv*, jobject, jint id, jboolean s) { aml->ShowToast(false, "Toggle %d: %s", (int)id, s?"ON":"OFF"); }
static void on_slider  (JNIEnv*, jobject, jint id, jint v)     { aml->ShowToast(false, "Slider %d: %d", (int)id, (int)v); }
static void on_checkbox(JNIEnv*, jobject, jint id, jboolean s) { aml->ShowToast(false, "CheckBox %d: %s", (int)id, s?"true":"false"); }
static void on_edit(JNIEnv* env, jobject, jint id, jstring jt) {
    const char* t = env->GetStringUTFChars(jt, nullptr);
    aml->ShowToast(false, "Input %d: %s", (int)id, t);
    env->ReleaseStringUTFChars(jt, t);
}

static void* init_thread(void*) {
    usleep(2000000);
    logf("[MM] thread start");

    if (!g_jvm) { logf("[MM] ERROR: g_jvm null"); g_status = -1; return nullptr; }

    JNIEnv* env = nullptr;
    if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
        logf("[MM] ERROR: attach gagal"); g_status = -1; return nullptr;
    }

    // ClassLoader
    jclass    clsCL  = env->FindClass("java/lang/ClassLoader");
    jmethodID midSCL = env->GetStaticMethodID(clsCL, "getSystemClassLoader",
                           "()Ljava/lang/ClassLoader;");
    jobject parentCL = env->CallStaticObjectMethod(clsCL, midSCL);
    if (!parentCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: parentCL null");
        g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }

    // Path
    const char* cfgPath = aml->GetConfigPath();
    char cleanCfg[256];
    strncpy(cleanCfg, cfgPath, sizeof(cleanCfg)-1);
    int cfgLen = strlen(cleanCfg);
    if (cfgLen > 0 && cleanCfg[cfgLen-1] == '/') cleanCfg[cfgLen-1] = '\0';
    char dexPath[256], optPath[256];
    snprintf(dexPath, sizeof(dexPath), "%s/modmenu.dex", cleanCfg);
    snprintf(optPath, sizeof(optPath), "%s/modmenu_opt",  cleanCfg);

    FILE* f = fopen(dexPath, "r");
    if (!f) { logf("[MM] ERROR: dex tidak ada"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr; }
    fclose(f);
    struct stat st;
    if (stat(optPath, &st) != 0) mkdir(optPath, 0755);

    // DexClassLoader
    jclass    clsDCL  = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID midInit = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    jstring jDex = env->NewStringUTF(dexPath);
    jstring jOpt = env->NewStringUTF(optPath);
    jobject dcl  = env->NewObject(clsDCL, midInit, jDex, jOpt, (jstring)nullptr, parentCL);
    env->DeleteLocalRef(jDex);
    env->DeleteLocalRef(jOpt);
    if (!dcl || env->ExceptionCheck()) {
        env->ExceptionDescribe(); env->ExceptionClear();
        logf("[MM] ERROR: DCL gagal"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }

    // loadClass
    jmethodID midLC = env->GetMethodID(clsDCL, "loadClass",
                          "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring   jCls  = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass    cls   = (jclass)env->CallObjectMethod(dcl, midLC, jCls);
    env->DeleteLocalRef(jCls);
    if (!cls || env->ExceptionCheck()) {
        env->ExceptionDescribe(); env->ExceptionClear();
        logf("[MM] ERROR: loadClass gagal"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }
    logf("[MM] loadClass OK");

    // RegisterNatives
    JNINativeMethod methods[] = {
        { "nativeOnButton",      "(I)V",                   (void*)on_button   },
        { "nativeOnToggle",      "(IZ)V",                  (void*)on_toggle   },
        { "nativeOnSlider",      "(II)V",                  (void*)on_slider   },
        { "nativeOnCheckBox",    "(IZ)V",                  (void*)on_checkbox },
        { "nativeOnEditConfirm", "(ILjava/lang/String;)V", (void*)on_edit     },
    };
    if (env->RegisterNatives(cls, methods, 5) != 0 || env->ExceptionCheck()) {
        env->ExceptionDescribe(); env->ExceptionClear();
        logf("[MM] ERROR: RegisterNatives gagal"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }
    logf("[MM] RegisterNatives OK");

    // Context
    jobject ctx = aml->GetAppContextObject();
    if (!ctx) { logf("[MM] ERROR: ctx null"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr; }

    // NewObject ModMenuHelper
    jmethodID midCtor = env->GetMethodID(cls, "<init>", "(Landroid/content/Context;)V");
    if (!midCtor || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: ctor null"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }
    jobject helper = env->NewObject(cls, midCtor, ctx);
    if (!helper || env->ExceptionCheck()) {
        env->ExceptionDescribe(); env->ExceptionClear();
        logf("[MM] ERROR: NewObject helper gagal"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }
    logf("[MM] helper OK");

    // Panggil show() — ini post ke UI thread via Handler internal ModMenuHelper
    jmethodID midShow = env->GetMethodID(cls, "show", "()V");
    env->CallVoidMethod(helper, midShow);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe(); env->ExceptionClear();
        logf("[MM] ERROR: show() exception"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }
    logf("[MM] show() OK");

    g_jvm->DetachCurrentThread();
    g_status = 1;
    return nullptr;
}

ON_MOD_PRELOAD() {
    remove(LOGFILE);
    g_jvm = nullptr; g_status = 0;
    logf("[MM] OnModPreLoad OK");
}

ON_MOD_LOAD() {
    logf("[MM] OnModLoad");
    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env) { logf("[MM] ERROR: env null"); return; }
    env->GetJavaVM(&g_jvm);
    pthread_t tid;
    if (pthread_create(&tid, nullptr, init_thread, nullptr) == 0)
        pthread_detach(tid);
}

ON_ALL_MODS_LOAD() {
    // Tunggu init_thread max 8 detik
    for (int i = 0; i < 80; i++) {
        if (g_status != 0) break;
        usleep(100000);
    }
    if (g_status == 1)
        aml->ShowToast(false, "[ModMenu] Aktif! Tap ☰ untuk buka panel");
    else
        aml->ShowToast(true, "[ModMenu] Gagal! status=%d", g_status);
    logf("[MM] OnAllModsLoaded status=%d", g_status);
}
