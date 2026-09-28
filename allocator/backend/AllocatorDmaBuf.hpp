/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ALLOCATOR_DMA_BUF_HPP
#define ALLOCATOR_DMA_BUF_HPP

#include "../Allocator.h"

using gralloc_generic::AllocatorBackendImpl;

class AllocatorBackendDmaBuf final : public AllocatorBackendImpl {
public:
	bool isReady() const override;
	int init() override;
	ndk::ScopedAStatus allocate2(const BufferDescriptorInfo& in_descriptor, int32_t in_count, AllocationResult* _aidl_return) override;
	ndk::ScopedAStatus isSupported(const BufferDescriptorInfo& in_descriptor, bool* _aidl_return) override;
private:
	ndk::ScopedAStatus grallocGbmAllocate(allocator_desc_t& desc, int32_t count, AllocationResult* outResult);
};

#endif /* ALLOCATOR_DMA_BUF_HPP */