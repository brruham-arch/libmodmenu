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
static int     g_status = 0; // 0=idle, 1=ok, -1=fail

static void* init_thread(void*) {
    usleep(3000000);
    logf("[MM] thread start");

    if (!g_jvm) { logf("[MM] ERROR: g_jvm null"); g_status = -1; return nullptr; }

    JNIEnv* env = nullptr;
    if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
        logf("[MM] ERROR: attach gagal"); g_status = -1; return nullptr;
    }

    jclass    clsCL  = env->FindClass("java/lang/ClassLoader");
    jmethodID midSCL = env->GetStaticMethodID(clsCL, "getSystemClassLoader",
                           "()Ljava/lang/ClassLoader;");
    jobject parentCL = env->CallStaticObjectMethod(clsCL, midSCL);
    if (!parentCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: parentCL null");
        g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }

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

    jclass    clsDCL  = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID midInit = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    jstring jDex = env->NewStringUTF(dexPath);
    jstring jOpt = env->NewStringUTF(optPath);
    jobject dcl  = env->NewObject(clsDCL, midInit,
                       jDex, jOpt, (jstring)nullptr, parentCL);
    env->DeleteLocalRef(jDex);
    env->DeleteLocalRef(jOpt);
    if (!dcl || env->ExceptionCheck()) {
        env->ExceptionDescribe(); env->ExceptionClear();
        logf("[MM] ERROR: DCL gagal"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }

    jmethodID midLC  = env->GetMethodID(clsDCL, "loadClass",
                           "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring   jCls   = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass    cls    = (jclass)env->CallObjectMethod(dcl, midLC, jCls);
    env->DeleteLocalRef(jCls);
    if (!cls || env->ExceptionCheck()) {
        env->ExceptionDescribe(); env->ExceptionClear();
        logf("[MM] ERROR: loadClass gagal"); g_jvm->DetachCurrentThread(); g_status = -1; return nullptr;
    }
    logf("[MM] loadClass OK");

    g_jvm->DetachCurrentThread();
    g_status = 1; // sinyal ke polling loop bahwa DEX berhasil
    return nullptr;
}

// Polling loop di main thread — aman untuk ShowToast
static void* toast_poll_thread(void*) {
    for (int i = 0; i < 100; i++) { // max 10 detik
        usleep(100000);
        if (g_status == 1) {
            aml->ShowToast(false, "[ModMenu] DEX loaded OK!");
            logf("[MM] toast OK");
            return nullptr;
        } else if (g_status == -1) {
            aml->ShowToast(true, "[ModMenu] Gagal load DEX!");
            return nullptr;
        }
    }
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
    logf("[MM] jvm: %p", g_jvm);

    pthread_t tid1, tid2;
    if (pthread_create(&tid1, nullptr, init_thread, nullptr) == 0)
        pthread_detach(tid1);
    if (pthread_create(&tid2, nullptr, toast_poll_thread, nullptr) == 0)
        pthread_detach(tid2);
}
