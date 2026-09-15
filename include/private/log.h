/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PRIVATE_LOG_H
#define _PRIVATE_LOG_H

#if !defined(NO_DEBUG) && defined(ANDROID_DEBUGGABLE)
//#warning Enabled verbose output.
#define LOG_NDEBUG 0
#endif

#ifndef LOG_TAG
#define LOG_TAG "GrallocGbmLog"
#warning The LOG_TAG is not defined! Using default tag.
#endif

#include <android/log.h>

#define LOG_TRACE() __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, "%s:%d func %s()", __FILE_NAME__, __LINE__, __FUNCTION__)

#define LOG_V(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOG_D(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_W(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOG_F(...) __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__)

#define LOG_ASSERT(cond, ...) __android_log_assert(cond, LOG_TAG, __VA_ARGS__)

#endif /* _PRIVATE_LOG_H */