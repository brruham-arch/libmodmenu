#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <jni.h>
#include "mod/amlmod.h"

MYMOD(brruham.modmenu, ModMenu, 1.0, brruham)

// ── Log ke file ───────────────────────────────────────────────────────────────
#define LOGFILE "/storage/emulated/0/modmenu_log.txt"

static void log_impl(const char* msg) {
    FILE* f = fopen(LOGFILE, "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}
static void logf(const char* fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    log_impl(buf);
}

// ── State ─────────────────────────────────────────────────────────────────────
static jobject g_helper   = nullptr;
static jclass  g_cls      = nullptr;
static bool    g_ready    = false;

// ── Native callbacks dari Java ────────────────────────────────────────────────
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnButton(
        JNIEnv*, jobject, jint id) {
    aml->ShowToast(false, "Button %d ditekan", (int)id);
}
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnToggle(
        JNIEnv*, jobject, jint id, jboolean state) {
    aml->ShowToast(false, "Toggle %d: %s", (int)id, state ? "ON" : "OFF");
}
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnSlider(
        JNIEnv*, jobject, jint id, jint value) {
    aml->ShowToast(false, "Slider %d: %d", (int)id, (int)value);
}
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnCheckBox(
        JNIEnv*, jobject, jint id, jboolean state) {
    aml->ShowToast(false, "CheckBox %d: %s", (int)id, state ? "true" : "false");
}
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnEditConfirm(
        JNIEnv* env, jobject, jint id, jstring jtext) {
    const char* text = env->GetStringUTFChars(jtext, nullptr);
    aml->ShowToast(false, "Input %d: %s", (int)id, text);
    env->ReleaseStringUTFChars(jtext, text);
}

// ── Update label dari C++ ─────────────────────────────────────────────────────
void modmenu_update_label(int id, const char* text) {
    if (!g_ready || !g_helper) return;
    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env) return;
    jmethodID mid = env->GetMethodID(g_cls, "updateLabel", "(ILjava/lang/String;)V");
    if (!mid) return;
    jstring jtext = env->NewStringUTF(text);
    env->CallVoidMethod(g_helper, mid, (jint)id, jtext);
    env->DeleteLocalRef(jtext);
}

// ── Init thread ───────────────────────────────────────────────────────────────
static void* init_thread(void*) {
    logf("[MM] init_thread start, tunggu 3 detik...");
    usleep(3000000);

    logf("[MM] Ambil JNIEnv...");
    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env) { logf("[MM] ERROR: JNIEnv null"); return nullptr; }
    logf("[MM] JNIEnv OK: %p", env);

    logf("[MM] Ambil Context...");
    jobject ctx = aml->GetAppContextObject();
    if (!ctx) { logf("[MM] ERROR: Context null"); return nullptr; }
    logf("[MM] Context OK: %p", ctx);

    // Path dex
    char dexPath[256], optPath[256];
    snprintf(dexPath, sizeof(dexPath), "%s/modmenu.dex", aml->GetConfigPath());
    snprintf(optPath, sizeof(optPath), "%s/modmenu_opt",  aml->GetConfigPath());
    logf("[MM] dexPath: %s", dexPath);

    // Cek file ada
    FILE* f = fopen(dexPath, "r");
    if (!f) {
        logf("[MM] ERROR: modmenu.dex tidak ditemukan");
        aml->ShowToast(true, "[ModMenu] modmenu.dex tidak ditemukan!");
        return nullptr;
    }
    fclose(f);
    logf("[MM] modmenu.dex ditemukan");

    // Ambil parent ClassLoader
    logf("[MM] Ambil ClassLoader...");
    jclass    clsCtx   = env->GetObjectClass(ctx);
    jmethodID midGCL   = env->GetMethodID(clsCtx, "getClassLoader",
                             "()Ljava/lang/ClassLoader;");
    if (!midGCL) { logf("[MM] ERROR: getClassLoader method null"); return nullptr; }
    jobject parentCL = env->CallObjectMethod(ctx, midGCL);
    if (!parentCL) { logf("[MM] ERROR: parentCL null"); return nullptr; }
    logf("[MM] parentCL OK: %p", parentCL);

    // Buat DexClassLoader
    logf("[MM] Buat DexClassLoader...");
    jclass clsDCL = env->FindClass("dalvik/system/DexClassLoader");
    if (!clsDCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass DexClassLoader gagal");
        return nullptr;
    }
    logf("[MM] FindClass DexClassLoader OK");

    jmethodID midDCLi = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if (!midDCLi) {
        logf("[MM] ERROR: GetMethodID DexClassLoader.<init> gagal");
        env->ExceptionClear();
        return nullptr;
    }
    logf("[MM] GetMethodID DexClassLoader.<init> OK");

    jstring jDexPath = env->NewStringUTF(dexPath);
    jstring jOptPath = env->NewStringUTF(optPath);
    jobject dcl = env->NewObject(clsDCL, midDCLi,
                      jDexPath, jOptPath, (jstring)nullptr, parentCL);
    env->DeleteLocalRef(jDexPath);
    env->DeleteLocalRef(jOptPath);

    if (!dcl || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: NewObject DexClassLoader gagal");
        return nullptr;
    }
    logf("[MM] DexClassLoader instance OK: %p", dcl);

    // loadClass
    logf("[MM] loadClass ModMenuHelper...");
    jmethodID midLC = env->GetMethodID(clsDCL, "loadClass",
                          "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!midLC) { logf("[MM] ERROR: GetMethodID loadClass gagal"); return nullptr; }

    jstring jClsName = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass  cls      = (jclass)env->CallObjectMethod(dcl, midLC, jClsName);
    env->DeleteLocalRef(jClsName);

    if (!cls || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: loadClass ModMenuHelper gagal");
        aml->ShowToast(true, "[ModMenu] Gagal load class!");
        return nullptr;
    }
    logf("[MM] loadClass OK: %p", cls);

    // Register native methods
    logf("[MM] RegisterNatives...");
    JNINativeMethod methods[] = {
        { "nativeOnButton",      "(I)V",
          (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnButton      },
        { "nativeOnToggle",      "(IZ)V",
          (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnToggle      },
        { "nativeOnSlider",      "(II)V",
          (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnSlider      },
        { "nativeOnCheckBox",    "(IZ)V",
          (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnCheckBox    },
        { "nativeOnEditConfirm", "(ILjava/lang/String;)V",
          (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnEditConfirm },
    };
    int regRet = env->RegisterNatives(cls, methods, 5);
    if (regRet != 0 || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: RegisterNatives gagal (ret=%d)", regRet);
        return nullptr;
    }
    logf("[MM] RegisterNatives OK");

    // Buat instance ModMenuHelper(context)
    logf("[MM] GetMethodID <init>...");
    jmethodID midInit = env->GetMethodID(cls, "<init>",
                            "(Landroid/content/Context;)V");
    if (!midInit || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: GetMethodID <init> gagal");
        return nullptr;
    }
    logf("[MM] GetMethodID <init> OK");

    logf("[MM] NewObject ModMenuHelper...");
    jobject helper = env->NewObject(cls, midInit, ctx);
    if (!helper || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: NewObject ModMenuHelper gagal");
        aml->ShowToast(true, "[ModMenu] Gagal buat instance!");
        return nullptr;
    }
    logf("[MM] ModMenuHelper instance OK: %p", helper);

    // Simpan global ref
    g_cls    = (jclass)env->NewGlobalRef(cls);
    g_helper = env->NewGlobalRef(helper);
    g_ready  = true;
    logf("[MM] Global refs OK");

    // Panggil show()
    logf("[MM] Panggil show()...");
    jmethodID midShow = env->GetMethodID(g_cls, "show", "()V");
    if (!midShow) { logf("[MM] ERROR: GetMethodID show gagal"); return nullptr; }
    env->CallVoidMethod(g_helper, midShow);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: show() exception");
        return nullptr;
    }
    logf("[MM] show() OK - ModMenu aktif!");

    aml->ShowToast(false, "[ModMenu] Aktif - tap ☰ untuk buka panel");
    return nullptr;
}

// ── Entry points ──────────────────────────────────────────────────────────────
ON_MOD_PRELOAD() {
    remove(LOGFILE);
    g_helper = nullptr;
    g_cls    = nullptr;
    g_ready  = false;
    logf("[MM] OnModPreLoad v1.0");
}

ON_MOD_LOAD() {
    logf("[MM] OnModLoad - start init_thread");
    pthread_t tid;
    if (pthread_create(&tid, nullptr, init_thread, nullptr) == 0) {
        pthread_detach(tid);
        logf("[MM] init_thread launched");
    } else {
        logf("[MM] ERROR: pthread_create gagal");
    }
}
