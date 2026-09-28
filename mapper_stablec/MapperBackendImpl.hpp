/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAPPER_BACKEND_IMPL_HPP
#define MAPPER_BACKEND_IMPL_HPP

#include <aidl/android/hardware/graphics/common/Cta861_3.h>
#include <aidl/android/hardware/graphics/common/Smpte2086.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/hardware/graphics/mapper/utils/IMapperProvider.h>
#include <cutils/native_handle.h>
#include <gralloc_generic.h>
#include <gralloctypes/Gralloc4.h>

enum MapperBackendType : uint8_t {
	MAPPER_GRALLOC_GBM = 0,
	MAPPER_GRALLOC_DMABUF,
	MAPPER_GRALLOC_UNKNOWN = UINT8_MAX
};

namespace gralloc_generic {

class MapperBackendImplV5 {
public:
	explicit MapperBackendImplV5() = default;
	virtual ~MapperBackendImplV5() = default;

	virtual bool isReady() const = 0;
	virtual int init() = 0;

	virtual int importBuffer(const native_handle_t* _Nonnull handle) = 0;
	virtual int freeBuffer(buffer_handle_t _Nonnull buffer) = 0;

	virtual int lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
			 int acquireFence, void* _Nullable* _Nonnull outData) = 0;
	virtual int unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence) = 0;

	virtual int flushLockedBuffer(buffer_handle_t _Nonnull buffer) = 0;
	virtual int rereadLockedBuffer(buffer_handle_t _Nonnull buffer) = 0;

	virtual int getStandardMetadata(buffer_handle_t _Nonnull buffer,
					int64_t standardMetadataType,
					void* _Nullable destBuffer, size_t destBufferSize) = 0;
	virtual int setStandardMetadata(buffer_handle_t _Nonnull buffer,
					int64_t standardMetadataType,
					buffer_metadata_t* _Nonnull metadata) = 0;
};

using MapperBackendImpl = MapperBackendImplV5;

} // namespace gralloc_generic

#endif /* MAPPER_BACKEND_IMPL_HPP */
