/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <aidl/android/hardware/graphics/allocator/AllocationError.h>
#include <aidl/android/hardware/graphics/allocator/AllocationResult.h>
#include <aidl/android/hardware/graphics/allocator/BnAllocator.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <android/hardware/graphics/common/1.2/types.h>

#include <gralloc_generic.h>

using aidl::android::hardware::common::NativeHandle;
using aidl::android::hardware::graphics::allocator::AllocationError;
using aidl::android::hardware::graphics::allocator::AllocationResult;
using aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using aidl::android::hardware::graphics::common::PixelFormat;
using aidl::android::hardware::graphics::common::BufferUsage;

enum AllocatorBackendType : uint8_t {
	ALLOCATOR_GRALLOC_GBM = 0,
	ALLOCATOR_GRALLOC_DMABUF,
	ALLOCATOR_GRALLOC_UNKNOWN = UINT8_MAX
};

namespace gralloc_generic {

class AllocatorBackendImplV2 {
public:
	explicit AllocatorBackendImplV2() = default;
	virtual ~AllocatorBackendImplV2() = default;

	virtual bool isReady() const = 0;
	virtual int init() = 0;
	virtual ndk::ScopedAStatus allocate2(const BufferDescriptorInfo& in_descriptor, int32_t in_count, AllocationResult* _aidl_return) = 0;
	virtual ndk::ScopedAStatus isSupported(const BufferDescriptorInfo& in_descriptor, bool* _aidl_return) = 0;

protected:
	ndk::ScopedAStatus ToBinderStatus(AllocationError error) {
		return ndk::ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(error));
	}

	int generateGrallocGenericDesc(const BufferDescriptorInfo& info, allocator_desc_t* outResult);
};

using AllocatorBackendImpl = AllocatorBackendImplV2;

} // namespace gralloc_generic

using gralloc_generic::AllocatorBackendImpl;

namespace aidl::android::hardware::graphics::allocator::impl {

class GrallocGenericAllocatorV2 : public BnAllocator {
public:
	GrallocGenericAllocatorV2() = default;
	~GrallocGenericAllocatorV2() = default;

	ndk::ScopedAStatus allocate(const std::vector<uint8_t>& descriptor, int32_t count,
				    allocator::AllocationResult* outResult) override;

	ndk::ScopedAStatus allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
				     allocator::AllocationResult* outResult) override;

	ndk::ScopedAStatus isSupported(const BufferDescriptorInfo& descriptor,
				       bool* outResult) override;

	ndk::ScopedAStatus getIMapperLibrarySuffix(std::string* outResult) override;

	int init(void);

protected:
	ndk::SpAIBinder createBinder() override;protected:
	ndk::ScopedAStatus ToBinderStatus(AllocationError error) {
		return ndk::ScopedAStatus::fromServiceSpecificError(static_cast<int32_t>(error));
	}

private:
	/// returns a shared-singleton Gralloc GBM backend
	std::shared_ptr<AllocatorBackendImpl> fetchBackendGrallocGbm();
	/// returns a shared-singleton DMA-BUF backend
	std::shared_ptr<AllocatorBackendImpl> fetchBackendDmaBuf();

	std::shared_ptr<AllocatorBackendImpl> mBackend;
	std::shared_ptr<AllocatorBackendImpl> selectBackendByType(const AllocatorBackendType type);
	int selectBackend(const BufferDescriptorInfo& descriptor);
};

} // namespace aidl::android::hardware::graphics::allocator::impl


#endif /* ALLOCATOR_H */