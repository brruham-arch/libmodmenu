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

static JavaVM* g_jvm = nullptr;

static void* init_thread(void*) {
    usleep(3000000);
    logf("[MM] thread start");

    if (!g_jvm) { logf("[MM] ERROR: g_jvm null"); return nullptr; }

    JNIEnv* env = nullptr;
    jint ret = g_jvm->AttachCurrentThread(&env, nullptr);
    logf("[MM] AttachCurrentThread ret=%d env=%p", ret, env);
    if (ret != JNI_OK || !env) { logf("[MM] ERROR: attach gagal"); return nullptr; }

    // ClassLoader via Thread
    jclass    clsThread = env->FindClass("java/lang/Thread");
    jmethodID midCT     = env->GetStaticMethodID(clsThread, "currentThread",
                              "()Ljava/lang/Thread;");
    jmethodID midGCCL   = env->GetMethodID(clsThread, "getContextClassLoader",
                              "()Ljava/lang/ClassLoader;");
    jobject curThread = env->CallStaticObjectMethod(clsThread, midCT);
    jobject parentCL  = env->CallObjectMethod(curThread, midGCCL);
    logf("[MM] parentCL: %p", parentCL);

    if (!parentCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] fallback: getSystemClassLoader");
        jclass    clsCL  = env->FindClass("java/lang/ClassLoader");
        jmethodID midSCL = env->GetStaticMethodID(clsCL, "getSystemClassLoader",
                               "()Ljava/lang/ClassLoader;");
        parentCL = env->CallStaticObjectMethod(clsCL, midSCL);
        logf("[MM] systemCL: %p", parentCL);
        if (!parentCL || env->ExceptionCheck()) {
            env->ExceptionClear();
            logf("[MM] ERROR: parentCL null");
            g_jvm->DetachCurrentThread();
            return nullptr;
        }
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
    logf("[MM] dex: %s", dexPath);

    FILE* f = fopen(dexPath, "r");
    if (!f) { logf("[MM] ERROR: dex tidak ada"); g_jvm->DetachCurrentThread(); return nullptr; }
    fclose(f);

    struct stat st;
    if (stat(optPath, &st) != 0) mkdir(optPath, 0755);

    // DexClassLoader
    jclass clsDCL = env->FindClass("dalvik/system/DexClassLoader");
    if (!clsDCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass DCL gagal");
        g_jvm->DetachCurrentThread();
        return nullptr;
    }
    jmethodID midDCLi = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if (!midDCLi || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: DCL.<init> null");
        g_jvm->DetachCurrentThread();
        return nullptr;
    }
    logf("[MM] DCL ready");

    jstring jDex = env->NewStringUTF(dexPath);
    jstring jOpt = env->NewStringUTF(optPath);
    jobject dcl  = env->NewObject(clsDCL, midDCLi,
                       jDex, (jstring)nullptr, (jstring)nullptr, parentCL);
    env->DeleteLocalRef(jDex);
    env->DeleteLocalRef(jOpt);

    if (!dcl || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: NewObject DCL gagal");
        g_jvm->DetachCurrentThread();
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
        g_jvm->DetachCurrentThread();
        return nullptr;
    }
    logf("[MM] loadClass OK!");
    aml->ShowToast(false, "[ModMenu] DEX loaded OK!");
    g_jvm->DetachCurrentThread();
    return nullptr;
}

ON_MOD_PRELOAD() {
    remove(LOGFILE);
    g_jvm = nullptr;
    logf("[MM] OnModPreLoad OK");
}

ON_MOD_LOAD() {
    logf("[MM] OnModLoad");

    // Ambil JVM dari env AML — paling reliable
    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env) { logf("[MM] ERROR: env null"); return; }
    logf("[MM] env: %p", env);

    // GetJavaVM dari env langsung
    JavaVM* jvm = nullptr;
    jint ret = env->GetJavaVM(&jvm);
    logf("[MM] GetJavaVM ret=%d jvm=%p", ret, jvm);
    if (ret != JNI_OK || !jvm) { logf("[MM] ERROR: GetJavaVM gagal"); return; }
    g_jvm = jvm;

    pthread_t tid;
    if (pthread_create(&tid, nullptr, init_thread, nullptr) == 0)
        pthread_detach(tid);
    logf("[MM] thread launched");
}
