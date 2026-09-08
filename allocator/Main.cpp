/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#define LOG_TAG "GrallocGbmAllocator"
#include <cutils/log.h>

#include "Allocator.h"

using aidl::android::hardware::graphics::allocator::impl::GrallocGbmAllocatorV2;

int main(int /*argc*/, char** /*argv*/) {
	ALOGI("GBM Mesa AIDL allocator starting up...");

	// same as SF main thread
	struct sched_param param = {0};
	param.sched_priority = 2;
	if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param) != 0) {
		ALOGI("%s: failed to set priority: %s", __FUNCTION__, strerror(errno));
	}

	auto allocator = ndk::SharedRefBase::make<GrallocGbmAllocatorV2>();
	CHECK(allocator != nullptr);

	if (allocator->init()) {
		ALOGE("Failed to initialize GBM Allocator.");
		return EXIT_FAILURE;
	}

	const std::string instance = std::string() + GrallocGbmAllocatorV2::descriptor + "/default";
	binder_status_t status =
		AServiceManager_addService(allocator->asBinder().get(), instance.c_str());
	CHECK_EQ(status, STATUS_OK);

	ABinderProcess_setThreadPoolMaxThreadCount(4);
	ABinderProcess_startThreadPool();
	ABinderProcess_joinThreadPool();

	return EXIT_FAILURE;
}