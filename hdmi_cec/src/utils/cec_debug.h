#ifndef UTILS_CEC_DEBUG_H_
#define UTILS_CEC_DEBUG_H_

#include <cutils/log.h>
#include <utils/Trace.h>

#ifndef DEBUG_LOG_TAG
#error "DEBUG_LOG_TAG is not defined!!"
#endif

#define HWCEC_LOGV(x, ...) ALOGV("[%s] " x, DEBUG_LOG_TAG, ##__VA_ARGS__)
#define HWCEC_LOGD(x, ...) ALOGD("[%s] " x, DEBUG_LOG_TAG, ##__VA_ARGS__)

#define HWCEC_LOGI(x, ...) ALOGI("[%s] " x, DEBUG_LOG_TAG, ##__VA_ARGS__)
#define HWCEC_LOGW(x, ...) ALOGW("[%s] " x, DEBUG_LOG_TAG, ##__VA_ARGS__)
#define HWCEC_LOGE(x, ...) ALOGE("[%s] " x, DEBUG_LOG_TAG, ##__VA_ARGS__)

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

enum HWC_DEBUG_COMPOSE_LEVEL
{
    COMPOSE_ENABLE_ALL  = 0,
    COMPOSE_DISABLE_MM  = 1,
    COMPOSE_DISABLE_UI  = 2,
    COMPOSE_DISABLE_ALL = 3
};


#ifdef USE_SYSTRACE
#define HWCEC_ATRACE_CALL() android::ScopedTrace ___tracer(ATRACE_TAG, __FUNCTION__)
#define HWCEC_ATRACE_NAME(name) android::ScopedTrace ___tracer(ATRACE_TAG, name)
#define HWCEC_ATRACE_INT(name, value) atrace_int(ATRACE_TAG, name, value)
#define HWCEC_ATRACE_ASYNC_BEGIN(name, cookie) atrace_async_begin(ATRACE_TAG, name, cookie)
#define HWCEC_ATRACE_ASYNC_END(name, cookie) atrace_async_end(ATRACE_TAG, name, cookie)
#else
#define HWCEC_ATRACE_CALL()
#define HWCEC_ATRACE_NAME(name)
#define HWCEC_ATRACE_INT(name, value)
#define HWCEC_ATRACE_ASYNC_BEGIN(name, cookie)
#define HWCEC_ATRACE_ASYNC_END(name, cookie)
#endif // USE_SYSTRACE

#endif // UTILS_CEC_DEBUG_H_