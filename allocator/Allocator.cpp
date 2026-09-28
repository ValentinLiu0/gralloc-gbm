/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android/binder_ibinder_platform.h>
#include <gralloctypes/Gralloc4.h>

#include "backend/AllocatorGrallocGbm.hpp"
#include "Allocator.h"

#define LOG_TAG "GrallocGenericAllocatorV2"
#include <private/log.h>

#define CHECK_BACKEND()	\
	do {		\
		if (mBackend == nullptr) {	\
			LOG_E("%s failed: Invalid backend pointer", __func__);			\
			return ToBinderStatus(AllocationError::NO_RESOURCES);		\
		}	\
		if (!mBackend->isReady()) {	\
			if (mBackend->init()) {	\
				LOG_E("%s failed: Backend initialization failed", __func__);	\
				return ToBinderStatus(AllocationError::NO_RESOURCES);	\
			}	\
		}	\
	} while (0)

using aidl::android::hardware::common::NativeHandle;
using aidl::android::hardware::graphics::common::ExtendableType;
using BufferDescriptorInfoV4 = android::hardware::graphics::mapper::V4_0::IMapper::BufferDescriptorInfo;

static const std::string STANDARD_METADATA_DATASPACE = "android.hardware.graphics.common.Dataspace";

int AllocatorBackendImpl::generateGrallocGenericDesc(const BufferDescriptorInfo& info, allocator_desc_t* outResult)
{
	LOG_TRACE();
	if (!outResult) {
		LOG_E("generateGrallocGenericDesc failed: Invalid out pointer.");
		return -EINVAL;
	}

	if (info.width == 0 || info.height == 0) {
		LOG_E("generateGrallocGenericDesc failed: Invalid buffer descriptor: width or height is zero");
		return -EINVAL;
	}

	outResult->width = static_cast<uint32_t>(info.width);
	outResult->height = static_cast<uint32_t>(info.height);

	// TODO: Add multiple layer support.
	if (info.layerCount > 1) {
		LOG_E("generateGrallocGenericDesc failed: Failed to convert descriptor. Unsupported layerCount: %d", info.layerCount);
		return -EINVAL;
	}

	outResult->format = static_cast<uint32_t>(info.format);
	outResult->usage = static_cast<uint32_t>(info.usage);
	outResult->reserved_size = static_cast<uint32_t>(info.reservedSize);
	outResult->layer_count = static_cast<uint32_t>(info.layerCount);

	outResult->name = (unsigned char *) calloc((info.name.size() + 1), sizeof(unsigned char));
	memcpy(outResult->name, info.name.data(), info.name.size());

	return 0;
}

namespace aidl::android::hardware::graphics::allocator::impl {

std::shared_ptr<AllocatorBackendImpl> GrallocGenericAllocatorV2::fetchBackendGrallocGbm()
{
	static std::mutex mutex;
	static std::weak_ptr<AllocatorBackendImpl> gbmBackend;
	std::lock_guard<std::mutex> lock(mutex);
	std::shared_ptr<AllocatorBackendImpl> backend = gbmBackend.lock();
	if (backend == nullptr) {
		backend = std::make_shared<AllocatorBackendGrallocGbm>();
		gbmBackend = backend;
	}
	return backend;
}

std::shared_ptr<AllocatorBackendImpl> GrallocGenericAllocatorV2::selectBackendByType(const AllocatorBackendType type)
{
	std::shared_ptr<AllocatorBackendImpl> backend;
	switch (type) {
	case AllocatorBackendType::ALLOCATOR_GRALLOC_GBM:
	default:
		backend = fetchBackendGrallocGbm();
	}
	assert((backend != nullptr));
	return backend;
}

int GrallocGenericAllocatorV2::selectBackend(const BufferDescriptorInfo& descriptor)
{
	LOG_TRACE();

	assert(descriptor);
	std::shared_ptr<AllocatorBackendImpl> backend;
	bool supported = false;

	backend = selectBackendByType(AllocatorBackendType::ALLOCATOR_GRALLOC_GBM);
	backend->isSupported(descriptor, &supported);
	if (supported) {
		mBackend = backend;
		return 0;
	}

	return -EINVAL;
}

int GrallocGenericAllocatorV2::init(void)
{
	LOG_TRACE();

	return selectBackendByType(AllocatorBackendType::ALLOCATOR_GRALLOC_GBM)->init();
}

ndk::ScopedAStatus GrallocGenericAllocatorV2::allocate(const std::vector<uint8_t>& encodedDescriptor, int32_t count,
						       allocator::AllocationResult* outResult)
{
	LOG_TRACE();

	BufferDescriptorInfoV4 mapperV4Descriptor;
	int ret = ::android::gralloc4::decodeBufferDescriptorInfo(encodedDescriptor, &mapperV4Descriptor);
	if (ret) {
		LOG_E("allocate failed: call decodeBufferDescriptorInfo() failed, ret=%d.", ret);
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

ndk::ScopedAStatus GrallocGenericAllocatorV2::allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
							allocator::AllocationResult* outResult)
{
	LOG_TRACE();

	selectBackend(descriptor);
	CHECK_BACKEND();

	return mBackend->allocate2(descriptor, count, outResult);
}

ndk::ScopedAStatus GrallocGenericAllocatorV2::isSupported(const BufferDescriptorInfo& descriptor,
							  bool* outResult)
{
	LOG_TRACE();

	/* Deny all non-standard metadata options */
	for (const auto& option : descriptor.additionalOptions) {
		if (option.name != STANDARD_METADATA_DATASPACE) {
			*outResult = false;
			return ndk::ScopedAStatus::ok();
		}
	}

	selectBackend(descriptor);
	CHECK_BACKEND();

	return mBackend->isSupported(descriptor, outResult);
}

ndk::ScopedAStatus GrallocGenericAllocatorV2::getIMapperLibrarySuffix(std::string* outResult)
{
	LOG_TRACE();
	*outResult = "generic";
	return ndk::ScopedAStatus::ok();
}

::ndk::SpAIBinder GrallocGenericAllocatorV2::createBinder()
{
	auto binder = BnAllocator::createBinder();
	AIBinder_setInheritRt(binder.get(), true);
	return binder;
}

}  // namespace aidl::android::hardware::graphics::allocator::impl