#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
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

static jobject g_ctx      = nullptr;
static jobject g_parentCL = nullptr;

static void* init_thread(void*) {
    usleep(3000000);
    logf("[MM] thread start");

    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env)      { logf("[MM] ERROR: env null"); return nullptr; }
    if (!g_ctx)    { logf("[MM] ERROR: g_ctx null"); return nullptr; }
    if (!g_parentCL){ logf("[MM] ERROR: g_parentCL null"); return nullptr; }
    logf("[MM] env=%p ctx=%p cl=%p", env, g_ctx, g_parentCL);

    // Fix double slash
    const char* cfgPath = aml->GetConfigPath();
    char cleanCfg[256];
    strncpy(cleanCfg, cfgPath, sizeof(cleanCfg)-1);
    int cfgLen = strlen(cleanCfg);
    if (cfgLen > 0 && cleanCfg[cfgLen-1] == '/') cleanCfg[cfgLen-1] = '\0';

    char dexPath[256], optPath[256];
    snprintf(dexPath, sizeof(dexPath), "%s/modmenu.dex", cleanCfg);
    snprintf(optPath, sizeof(optPath), "%s/modmenu_opt",  cleanCfg);
    logf("[MM] dex: %s", dexPath);

    FILE* f = fopen(dexPath, "r");
    if (!f) { logf("[MM] ERROR: dex tidak ada"); return nullptr; }
    fclose(f);
    logf("[MM] dex ada");

    // DexClassLoader
    jclass clsDCL = env->FindClass("dalvik/system/DexClassLoader");
    if (!clsDCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass DexClassLoader gagal");
        return nullptr;
    }
    jmethodID midDCLi = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if (!midDCLi || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: DCL.<init> gagal");
        return nullptr;
    }
    logf("[MM] DCL ready");

    jstring jDex = env->NewStringUTF(dexPath);
    jstring jOpt = env->NewStringUTF(optPath);
    jobject dcl  = env->NewObject(clsDCL, midDCLi,
                       jDex, jOpt, (jstring)nullptr, g_parentCL);
    env->DeleteLocalRef(jDex);
    env->DeleteLocalRef(jOpt);

    if (!dcl || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: NewObject DCL gagal");
        return nullptr;
    }
    logf("[MM] DCL instance OK");

    jmethodID midLC  = env->GetMethodID(clsDCL, "loadClass",
                           "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jClsName = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass  cls      = (jclass)env->CallObjectMethod(dcl, midLC, jClsName);
    env->DeleteLocalRef(jClsName);

    if (!cls || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: loadClass gagal");
        return nullptr;
    }
    logf("[MM] loadClass OK");

    aml->ShowToast(false, "[ModMenu] DEX loaded OK!");
    return nullptr;
}

ON_MOD_PRELOAD() {
    remove(LOGFILE);
    g_ctx      = nullptr;
    g_parentCL = nullptr;
    logf("[MM] OnModPreLoad OK");
}

ON_MOD_LOAD() {
    logf("[MM] OnModLoad");

    // Ambil ctx dan ClassLoader di sini (main thread, paling aman)
    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env) { logf("[MM] ERROR: env null di OnModLoad"); return; }

    jobject ctx = aml->GetAppContextObject();
    if (!ctx) { logf("[MM] ERROR: ctx null di OnModLoad"); return; }

    // Simpan ctx sebagai global ref
    g_ctx = env->NewGlobalRef(ctx);
    logf("[MM] g_ctx: %p", g_ctx);

    // Ambil ClassLoader dari ctx di main thread
    jclass    clsCtx = env->FindClass("android/content/Context");
    jmethodID midGCL = env->GetMethodID(clsCtx, "getClassLoader",
                           "()Ljava/lang/ClassLoader;");
    if (!midGCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: getClassLoader method null");
        return;
    }

    jobject cl = env->CallObjectMethod(g_ctx, midGCL);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: getClassLoader() exception");
        return;
    }
    if (!cl) { logf("[MM] ERROR: cl null"); return; }

    g_parentCL = env->NewGlobalRef(cl);
    logf("[MM] g_parentCL: %p", g_parentCL);

    pthread_t tid;
    if (pthread_create(&tid, nullptr, init_thread, nullptr) == 0)
        pthread_detach(tid);
    logf("[MM] thread launched");
}
