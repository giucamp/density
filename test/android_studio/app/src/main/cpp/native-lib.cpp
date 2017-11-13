#include <jni.h>
#include <string>

int run(int argc, char **argv);

extern "C"
JNIEXPORT jstring

JNICALL
Java_com_example_giuca_densitytest_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */){

    run(0, nullptr);

    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
