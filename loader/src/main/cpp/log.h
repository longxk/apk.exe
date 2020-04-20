#include <android/log.h>

#define TAG	"APK.EXE"

#ifndef LOG_LEVEL
#define LOG_LEVEL ANDROID_LOG_DEBUG
#endif

#define LOG(level, ...) __android_log_print(level >= LOG_LEVEL ? level : 0, TAG, __VA_ARGS__)

#define LOGD(...) LOG(ANDROID_LOG_DEBUG, __VA_ARGS__)

#define LOGI(...) LOG(ANDROID_LOG_INFO, __VA_ARGS__)

#define LOGW(...) LOG(ANDROID_LOG_WARN, __VA_ARGS__)

#define	LOGE(...) LOG(ANDROID_LOG_ERROR, __VA_ARGS__)
