/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <aidl/android/hardware/graphics/allocator/AllocationResult.h>
#include <aidl/android/hardware/graphics/allocator/BnAllocator.h>
#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include <android/hardware/graphics/common/1.2/types.h>

#include <gralloc_gbm.h>

using aidl::android::hardware::common::NativeHandle;
using aidl::android::hardware::graphics::common::PixelFormat;
using aidl::android::hardware::graphics::common::BufferUsage;

namespace aidl::android::hardware::graphics::allocator::impl {

class GrallocGbmAllocatorV2 : public BnAllocator {
	public:
	GrallocGbmAllocatorV2() = default;
	~GrallocGbmAllocatorV2() = default;

	ndk::ScopedAStatus allocate(const std::vector<uint8_t>& descriptor, int32_t count,
				    allocator::AllocationResult* outResult) override;

	ndk::ScopedAStatus allocate2(const BufferDescriptorInfo& descriptor, int32_t count,
				     allocator::AllocationResult* outResult) override;

	ndk::ScopedAStatus isSupported(const BufferDescriptorInfo& descriptor,
				       bool* outResult) override;

	ndk::ScopedAStatus getIMapperLibrarySuffix(std::string* outResult) override;

	int init(void);

	protected:
	ndk::SpAIBinder createBinder() override;

	private:
	ndk::ScopedAStatus generateGrallocGbmDesc(const BufferDescriptorInfo& info, allocator_desc_t* outResult);

	ndk::ScopedAStatus grallocGbmAllocate(allocator_desc_t& desc, int32_t count, allocator::AllocationResult* outResult);
};

} // namespace aidl::android::hardware::graphics::allocator::impl


#endif /* ALLOCATOR_H */