/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <android-base/logging.h>
#include <android/binder_ibinder_platform.h>
#include <gralloctypes/Gralloc4.h>

#include "Allocator.h"

#define LOG_TAG "GrallocGbmAllocatorV2"
#include <cutils/log.h>

using aidl::android::hardware::common::NativeHandle;
using aidl::android::hardware::graphics::common::ExtendableType;
using BufferDescriptorInfoV4 = android::hardware::graphics::mapper::V4_0::IMapper::BufferDescriptorInfo;

static const std::string STANDARD_METADATA_DATASPACE = "android.hardware.graphics.common.Dataspace";

namespace aidl::android::hardware::graphics::allocator::impl {

inline ndk::ScopedAStatus ToBinderStatus(AllocationError error) {
	return ndk::ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(error));
}

int GrallocGbmAllocatorV2::init(void)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	return gralloc_gbm_init();
}

ndk::ScopedAStatus GrallocGbmAllocatorV2::generateGrallocGbmDesc(const BufferDescriptorInfo& info, allocator_desc_t* outResult)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!outResult) {
		ALOGE("generateGrallocGbmDesc failed: Invalid out pointer.");
		return ToBinderStatus(AllocationError::NO_RESOURCES);
	}

	if (info.width == 0 || info.height == 0) {
		ALOGE("generateGrallocGbmDesc failed: Invalid buffer descriptor: width or height is zero");
		return ToBinderStatus(AllocationError::BAD_DESCRIPTOR);
	}

	outResult->width = static_cast<uint32_t>(info.width);
	outResult->height = static_cast<uint32_t>(info.height);

	// TODO: Add multiple layer support.
	if (info.layerCount > 1) {
		ALOGE("generateGrallocGbmDesc failed: Failed to convert descriptor. Unsupported layerCount: %d", info.layerCount);
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}

	outResult->format = static_cast<uint32_t>(info.format);
	outResult->usage = static_cast<uint32_t>(info.usage);
	outResult->reserved_size = static_cast<uint32_t>(info.reservedSize);
	outResult->layer_count = static_cast<uint32_t>(info.layerCount);

	outResult->name = (unsigned char *) calloc(info.name.max_size(), sizeof(unsigned char));
	memcpy(outResult->name, info.name.data(), info.name.size());

	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GrallocGbmAllocatorV2::grallocGbmAllocate(allocator_desc_t& desc, int32_t count, 
							     allocator::AllocationResult* outResult)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("grallocGbmAllocate failed: Failed to initialize the gralloc_gbm driver");
			return ToBinderStatus(AllocationError::NO_RESOURCES);
		}
	}

	std::vector<native_handle_t *> handles;
	handles.resize(count, nullptr);
    
	int32_t pixel_stride = 0;
	for (int32_t i = 0; i < count; i++) {
		native_handle_t *handle;
		uint32_t gbm_stride = 0;
		int ret = gralloc_gbm_android_buffer_new(&desc, &gbm_stride, &handle);
		if (ret || !handle) {
			ALOGE("grallocGbmAllocate failed: GBM operation failed.");
			for (int32_t j = 0; j < i; j++) {
				// Release all buffer and handle
				if (!handles[j])
					continue;
				gralloc_gbm_android_buffer_free(handles[j]);
				native_handle_close(handles[j]);
				native_handle_delete(handles[j]);
			}
			return ToBinderStatus(AllocationError::UNSUPPORTED);
		}
		handles[i] = handle;
		pixel_stride = gralloc_gbm_caculate_android_pixel_stride(desc.format, gbm_stride);
	}

	outResult->buffers.resize(count);
	for (int32_t i = 0; i < count; i++) {
		if (!handles[i])
			continue;
		auto handle = handles[i];
		outResult->buffers[i] = ::android::dupToAidl(handle);
		// Release buffer and handle
		gralloc_gbm_android_buffer_free(handle);
		native_handle_close(handle);
		native_handle_delete(handle);
	}
	outResult->stride = pixel_stride;

	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GrallocGbmAllocatorV2::allocate(const std::vector<uint8_t>& encodedDescriptor, int32_t count,
						   allocator::AllocationResult* outResult)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("allocate failed: Failed to initialize the gralloc_gbm driver");
			return ToBinderStatus(AllocationError::NO_RESOURCES);
		}
	}

	BufferDescriptorInfoV4 mapperV4Descriptor;
	int ret = ::android::gralloc4::decodeBufferDescriptorInfo(encodedDescriptor, &mapperV4Descriptor);
	if (ret) {
		ALOGE("allocate failed: call decodeBufferDescriptorInfo() failed, ret=%d.", ret);
		return ToBinderStatus(AllocationError::BAD_DESCRIPTOR);
	}

	const BufferDescriptorInfo info = {
		.name = {"auto_generated"},
		.width = static_cast<int32_t>(mapperV4Descriptor.width),
		.height = static_cast<int32_t>(mapperV4Descriptor.height),
		.layerCount = static_cast<int32_t>(mapperV4Descriptor.layerCount),
		.format = (PixelFormat) mapperV4Descriptor.format,
		.usage = (BufferUsage) mapperV4Descriptor.usage,
		.reservedSize = static_cast<int64_t>(mapperV4Descriptor.reservedSize),
	};

	return allocate2(info, count, outResult);
}

ndk::ScopedAStatus GrallocGbmAllocatorV2::allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
						    allocator::AllocationResult* outResult)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	allocator_desc_t grallocGbmDesc = {};
	ndk::ScopedAStatus status = generateGrallocGbmDesc(descriptor, &grallocGbmDesc);
	if (!status.isOk()) {
		ALOGE("allocate2 failed: Failed to convert the request buffer desc to Gralloc GBM desc.\n");
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}

	if (!gralloc_gbm_is_allocator_desc_supported(&grallocGbmDesc)) {
		ALOGE("allocate2 failed: The requested desc is unsupported by the driver.\n");
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}

	return grallocGbmAllocate(grallocGbmDesc, count, outResult);
}

ndk::ScopedAStatus GrallocGbmAllocatorV2::isSupported(const BufferDescriptorInfo& descriptor,
						      bool* outResult)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("isSupported failed: Failed to initialize the gralloc_gbm driver");
			return ToBinderStatus(AllocationError::NO_RESOURCES);;
		}
	}

	/* Deny all non-standard metadata options */
	for (const auto& option : descriptor.additionalOptions) {
		if (option.name != STANDARD_METADATA_DATASPACE) {
			*outResult = false;
			return ndk::ScopedAStatus::ok();
		}
	}

	allocator_desc_t grallocGbmDesc = {};
	ndk::ScopedAStatus status = generateGrallocGbmDesc(descriptor, &grallocGbmDesc);
	if (!status.isOk()) {
		ALOGE("isSupported failed: Failed to convert the request buffer desc to Gralloc GBM desc.\n");
		return ToBinderStatus(AllocationError::UNSUPPORTED);
	}
	
	*outResult = gralloc_gbm_is_allocator_desc_supported(&grallocGbmDesc);
	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GrallocGbmAllocatorV2::getIMapperLibrarySuffix(std::string* outResult)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	*outResult = "gbm";
	return ndk::ScopedAStatus::ok();
}

::ndk::SpAIBinder GrallocGbmAllocatorV2::createBinder()
{
	auto binder = BnAllocator::createBinder();
	AIBinder_setInheritRt(binder.get(), true);
	return binder;
}

}  // namespace aidl::android::hardware::graphics::allocator::impl