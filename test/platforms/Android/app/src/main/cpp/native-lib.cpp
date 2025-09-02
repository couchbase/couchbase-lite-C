#include <jni.h>
#include <string>
#include <android/log.h>

#define CATCH_CONFIG_CONSOLE_WIDTH 80
#define CATCH_CONFIG_NOSTDOUT
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "CBLTest.hh"

using namespace std;
using namespace fleece;

class AndroidLog {
public:
    static constexpr const char* LOG_TAG = "CBLTests";

    static void log(CBLLogLevel level, string& message) {
        int logLevel;
        switch (level) {
            case kCBLLogDebug:
                logLevel = ANDROID_LOG_DEBUG;
                break;
            case kCBLLogVerbose:
                logLevel = ANDROID_LOG_VERBOSE;
                break;
            case kCBLLogInfo:
                logLevel = ANDROID_LOG_INFO;
                break;
            case kCBLLogWarning:
                logLevel = ANDROID_LOG_WARN;
                break;
            case kCBLLogError:
                logLevel = ANDROID_LOG_ERROR;
                break;
            default:
                logLevel = ANDROID_LOG_UNKNOWN;
        }
        __android_log_write(logLevel, LOG_TAG, message.c_str());
    }
};

class AndroidLogStream : public std::stringbuf {
public:
    AndroidLogStream() {}
    ~AndroidLogStream() { pubsync(); }
    int sync() {
        string line;
        stringstream ss(str());
        while (getline(ss, line, '\n')) {
            AndroidLog::log(kCBLLogInfo, line);
        }
        str(""); // Reset the buffer to avoid logging multiple times
        return 0;
    }
};

// Initialize the custom stream buffer
static AndroidLogStream androidLogStream;
void redirectCatchToLogcat() {
    std::cout.rdbuf(&androidLogStream);
    std::clog.rdbuf(&androidLogStream);
    std::cerr.rdbuf(&androidLogStream);
}

extern "C" JNIEXPORT int JNICALL
Java_com_couchbase_tests_CouchbaseLiteTest_runTests(
        JNIEnv* env,
        jobject /* this */,
        jstring filesDir,
        jstring tmpDir,
        jstring assetsDir,
        jobjectArray tests) {
    const char* cFilesDir = env->GetStringUTFChars(filesDir, NULL);
    const char* cTmpDir = env->GetStringUTFChars(tmpDir, NULL);
    const char* cAssetsDir = env->GetStringUTFChars(assetsDir, NULL);

    // Initialize Android Context:
    CBLTest::initAndroidContext({cFilesDir, cTmpDir, cAssetsDir});
    env->ReleaseStringUTFChars(filesDir, cFilesDir);
    env->ReleaseStringUTFChars(tmpDir, cTmpDir);
    env->ReleaseStringUTFChars(tmpDir, cAssetsDir);

    // Set custom logging:
    CBLLogSinkCallback callback = [](CBLLogDomain domain, CBLLogLevel level, FLString msg) {
        string m = slice(msg).asString();
        AndroidLog::log(level, m);
    };
    CBLLogSinks_SetCustom({ kCBLLogInfo, callback, kCBLLogDomainMaskAll});
    
    redirectCatchToLogcat();

    // Preparing Catch test arguments:
    vector<string> tmpArgs;
    tmpArgs.push_back("CBLTests"); // Just a fake execution file name

    int count = env->GetArrayLength(tests);
    for (int i = 0; i < count; i++) {
        jstring name = (jstring) env->GetObjectArrayElement(tests, i);
        const char *cName = env->GetStringUTFChars(name, NULL);
        tmpArgs.push_back(cName);
        env->ReleaseStringUTFChars(name, cName);
    }
    vector<const char *> args(tmpArgs.size());
    transform(tmpArgs.begin(), tmpArgs.end(), args.begin(), [](string& s) {
        return s.c_str();
    });

    // Start Catch test session:
    Catch::Session session;
    session.applyCommandLine(args.size(), args.data());
    return session.run();
}
