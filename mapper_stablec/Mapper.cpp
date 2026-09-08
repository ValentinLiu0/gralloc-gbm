/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gralloc_gbm.h>
#include <gralloc_gbm_format.h>

#define LOG_TAG "GrallocGbmMapperV5"
#include <cutils/log.h>

#include <aidl/android/hardware/graphics/allocator/BufferDescriptorInfo.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>
#include <android-base/unique_fd.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/hardware/graphics/mapper/utils/IMapperProvider.h>
#include <cutils/native_handle.h>
#include <gralloctypes/Gralloc4.h>

using namespace ::aidl::android::hardware::graphics::common;
using namespace ::android::hardware::graphics::mapper;
using ::aidl::android::hardware::graphics::allocator::BufferDescriptorInfo;
using ::android::base::unique_fd;

constexpr const char* STANDARD_METADATA_NAME =
        "android.hardware.graphics.common.StandardMetadataType";

inline bool is_standard_metadata(const AIMapper_MetadataType type)
{
	return (strcmp(STANDARD_METADATA_NAME, type.name) == 0);
}

inline bool is_native_handle_valid(const native_handle_t *handle)
{
	if (!handle || handle->numFds == 0) {
		return false;
	}
	return true;
}

class GrallocGbmMapperV5 final : public vendor::mapper::IMapperV5Impl {
	public:
	explicit GrallocGbmMapperV5() = default;
	~GrallocGbmMapperV5() = default;

	AIMapper_Error importBuffer(const native_handle_t* _Nonnull handle,
				buffer_handle_t _Nullable* _Nonnull outBufferHandle) override;

	AIMapper_Error freeBuffer(buffer_handle_t _Nonnull buffer) override;

	AIMapper_Error getTransportSize(buffer_handle_t _Nonnull buffer, uint32_t* _Nonnull outNumFds,
					uint32_t* _Nonnull outNumInts) override;

	AIMapper_Error lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
			int acquireFence, void* _Nullable* _Nonnull outData) override;

	AIMapper_Error unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence) override;

	AIMapper_Error flushLockedBuffer(buffer_handle_t _Nonnull buffer) override;

	AIMapper_Error rereadLockedBuffer(buffer_handle_t _Nonnull buffer) override;

	int32_t getMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
			void* _Nonnull outData, size_t outDataSize) override;

	int32_t getStandardMetadata(buffer_handle_t _Nonnull buffer, int64_t standardMetadataType,
				void* _Nonnull outData, size_t outDataSize) override;

	AIMapper_Error setMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
				   const void* _Nonnull metadata, size_t metadataSize) override;

	AIMapper_Error setStandardMetadata(buffer_handle_t _Nonnull buffer,
					   int64_t standardMetadataType, const void* _Nonnull metadata,
					   size_t metadataSize) override;

	AIMapper_Error listSupportedMetadataTypes(
		const AIMapper_MetadataTypeDescription* _Nullable* _Nonnull outDescriptionList,
		size_t* _Nonnull outNumberOfDescriptions) override;

	AIMapper_Error dumpBuffer(buffer_handle_t _Nonnull bufferHandle,
				  AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
				  void* _Null_unspecified context) override;

	AIMapper_Error dumpAllBuffers(AIMapper_BeginDumpBufferCallback _Nonnull beginDumpBufferCallback,
				  AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
				  void* _Null_unspecified context) override;

	AIMapper_Error getReservedRegion(buffer_handle_t _Nonnull buffer,
					 void* _Nullable* _Nonnull outReservedRegion,
					 uint64_t* _Nonnull outReservedSize) override;
};

AIMapper_Error GrallocGbmMapperV5::importBuffer(const native_handle_t* _Nonnull handle,
						buffer_handle_t _Nullable* _Nonnull outBufferHandle)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!::is_native_handle_valid(handle)) {
		ALOGE("importBuffer failed: invalid handle (%p).", handle);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("importBuffer failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_NO_RESOURCES;
		}
	}

	native_handle_t *importedBufferHandle = native_handle_clone(handle);
	if (!importedBufferHandle) {
		ALOGE("importBuffer failed: handle clone failed");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	if (gralloc_gbm_android_buffer_import((buffer_handle_t) importedBufferHandle)) {
		native_handle_close(importedBufferHandle);
		native_handle_delete(importedBufferHandle);
		ALOGE("importBuffer failed: GBM operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	*outBufferHandle = (buffer_handle_t) importedBufferHandle;
	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::freeBuffer(buffer_handle_t _Nonnull buffer)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("freeBuffer failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("freeBuffer failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_NO_RESOURCES;
		}
	}

	if (gralloc_gbm_android_buffer_free(buffer)) {
		ALOGE("freeBuffer failed: GBM operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::getTransportSize(buffer_handle_t _Nonnull buffer, uint32_t* _Nonnull outNumFds,
						    uint32_t* _Nonnull outNumInts)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("lock failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("getTransportSize failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_NO_RESOURCES;
		}
	}

	*outNumFds = buffer->numFds;
	*outNumInts = buffer->numInts;
	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::lock(buffer_handle_t _Nonnull buffer, uint64_t cpuUsage, ARect accessRegion,
					int acquireFence, void* _Nullable* _Nonnull outData)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("lock failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (cpuUsage == 0) {
		ALOGE("lock failed: invalid usage.");
		return AIMAPPER_ERROR_BAD_VALUE;
	}

	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("lock failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_NO_RESOURCES;
		}
	}

	if (gralloc_gbm_android_buffer_lock(buffer, cpuUsage, accessRegion.top, accessRegion.bottom, accessRegion.left, accessRegion.right, outData)) {
		ALOGE("lock failed: GBM operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::unlock(buffer_handle_t _Nonnull buffer, int* _Nonnull releaseFence)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("unlock failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("unlock failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_NO_RESOURCES;
		}
	}
	
	if (gralloc_gbm_android_buffer_unlock(buffer)) {
		ALOGE("unlock failed: GBM operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::flushLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	ALOGD("flushLockedBuffer: no operations for GBM.");
	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::rereadLockedBuffer(buffer_handle_t _Nonnull buffer)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	ALOGD("rereadLockedBuffer: no operations for GBM.");
	return AIMAPPER_ERROR_NONE;
}

constexpr AIMapper_MetadataTypeDescription newStandardMetadata(StandardMetadataType type,
							       bool isGettable, bool isSettable)
{
	return {
		{STANDARD_METADATA_NAME, static_cast<int64_t>(type)},
		nullptr,
		isGettable,
		isSettable,
		{0}
	};
}

int32_t GrallocGbmMapperV5::getMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
					void* _Nonnull outData, size_t outDataSize)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("getMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (is_standard_metadata(metadataType))
		return getStandardMetadata(buffer, metadataType.value, outData, outDataSize);

	ALOGE("getMetadata failed: Non-standard metadata (%s) is unsupported!", metadataType.name);
	return AIMAPPER_ERROR_UNSUPPORTED;
}

inline int getPlaneLayouts(uint32_t gbm_format, std::vector<PlaneLayout>* outPlaneLayouts);

template <typename F, StandardMetadataType metadataType>
int32_t grallocGbmQueryAndroidBufferMetadata(buffer_handle_t handle, F&& provide,
					     StandardMetadata<metadataType>)
{
	android_buffer_info_t info;
	if (gralloc_gbm_android_buffer_query(handle, &info)) {
		ALOGE("grallocGbmQueryAndroidBufferMetadata failed: GBM operation failed.");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	if constexpr (metadataType == StandardMetadataType::BUFFER_ID) {
		return provide(reinterpret_cast<uint64_t>(info.buffer_id));
	}
	if constexpr (metadataType == StandardMetadataType::NAME) {
		if (info.name)
			return provide(reinterpret_cast<const char *>(info.name));

		return provide("<unknown name>");
	}
	if constexpr (metadataType == StandardMetadataType::WIDTH) {
		return provide(static_cast<int32_t>(info.width));
	}
	if constexpr (metadataType == StandardMetadataType::HEIGHT) {
		return provide(static_cast<int32_t>(info.height));
	}
	if constexpr (metadataType == StandardMetadataType::LAYER_COUNT) {
		return provide(static_cast<int32_t>(info.layer_count));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_REQUESTED) {
		return provide(static_cast<PixelFormat>(info.android_format));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_FOURCC) {
		return provide(static_cast<uint32_t>(info.gbm_format));
	}
	if constexpr (metadataType == StandardMetadataType::PIXEL_FORMAT_MODIFIER) {
		return provide(info.modifier);
	}
	if constexpr (metadataType == StandardMetadataType::USAGE) {
		return provide(static_cast<BufferUsage>(info.usage));
	}
	if constexpr (metadataType == StandardMetadataType::ALLOCATION_SIZE) {
		return provide(static_cast<uint64_t>(info.size));
	}
	if constexpr (metadataType == StandardMetadataType::PROTECTED_CONTENT) {
		return provide(static_cast<bool>(info.is_protected));
	}
	if constexpr (metadataType == StandardMetadataType::COMPRESSION) {
		return provide(android::gralloc4::Compression_None);
	}
	if constexpr (metadataType == StandardMetadataType::INTERLACED) {
		return provide(android::gralloc4::Interlaced_None);
	}
	if constexpr (metadataType == StandardMetadataType::CHROMA_SITING) {
		return provide(android::gralloc4::ChromaSiting_None);
	}
	if constexpr (metadataType == StandardMetadataType::PLANE_LAYOUTS) {
		int32_t num = info.plane_count;
		std::vector<PlaneLayout> planeLayouts;
		if (getPlaneLayouts(info.gbm_format, &planeLayouts)) {
			return AIMAPPER_ERROR_UNSUPPORTED;
		}

		for (size_t plane = 0; plane < planeLayouts.size(); plane++) {
			PlaneLayout& planeLayout = planeLayouts[plane];
			planeLayout.offsetInBytes = 0;
			planeLayout.strideInBytes = info.stride * color_bytes_per_pixel(info.gbm_format);
			// FIXME: vertical_subsampling=1 for now
			planeLayout.totalSizeInBytes = planeLayout.strideInBytes * DIV_ROUND_UP(info.height, 1);
			planeLayout.widthInSamples = info.width / planeLayout.horizontalSubsampling;
			planeLayout.heightInSamples = info.height / planeLayout.verticalSubsampling;
		}
        	return provide(planeLayouts);
	}
	if constexpr (metadataType == StandardMetadataType::CROP) {
		const uint32_t numPlanes = 1; // FIXME: We only support 1 currently
		std::vector<aidl::android::hardware::graphics::common::Rect> crops;
		for (uint32_t plane = 0; plane < numPlanes; plane++) {
			aidl::android::hardware::graphics::common::Rect crop;
			crop.left = 0;
			crop.top = 0;
			crop.right = info.width;
			crop.bottom = info.height;
			crops.push_back(crop);
		}
		return provide(crops);
	}
	if constexpr (metadataType == StandardMetadataType::DATASPACE) {
		return provide(static_cast<Dataspace>(info.dataspace));
	}
	if constexpr (metadataType == StandardMetadataType::BLEND_MODE) {
		return provide(static_cast<BlendMode>(info.dataspace));
	}
	if constexpr (metadataType == StandardMetadataType::SMPTE2086) {
		std::optional<Smpte2086> smpte;
		return AIMAPPER_ERROR_UNSUPPORTED;
	}
	if constexpr (metadataType == StandardMetadataType::CTA861_3) {
		std::optional<Cta861_3> cta;
		return AIMAPPER_ERROR_UNSUPPORTED;
	}
	if constexpr (metadataType == StandardMetadataType::SMPTE2094_40) {
		std::optional<Smpte2086> smpte;
		return AIMAPPER_ERROR_UNSUPPORTED;
	}
	if constexpr (metadataType == StandardMetadataType::SMPTE2094_10) {
		std::optional<Smpte2086> smpte;
		return AIMAPPER_ERROR_UNSUPPORTED;
	}
	if constexpr (metadataType == StandardMetadataType::STRIDE) {
		return provide(static_cast<int32_t>(info.stride));
	}

	ALOGW("Unknown metadata type: %s", toString(metadataType).c_str());
	return AIMAPPER_ERROR_UNSUPPORTED;
}

const std::unordered_map<uint32_t, std::vector<PlaneLayout>>& GetPlaneLayoutsMap()
{
    static const auto* kPlaneLayoutsMap =
            new std::unordered_map<uint32_t, std::vector<PlaneLayout>>({
                    {GBM_FORMAT_ABGR8888,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 8,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 16,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_A,
                                             .offsetInBits = 24,
                                             .sizeInBits = 8}},
                             .sampleIncrementInBits = 32,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_ABGR2101010,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 10},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 10,
                                             .sizeInBits = 10},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 20,
                                             .sizeInBits = 10},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_A,
                                             .offsetInBits = 30,
                                             .sizeInBits = 2}},
                             .sampleIncrementInBits = 32,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_ABGR16161616F,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 16},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 16,
                                             .sizeInBits = 16},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 32,
                                             .sizeInBits = 16},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_A,
                                             .offsetInBits = 48,
                                             .sizeInBits = 16}},
                             .sampleIncrementInBits = 64,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_ARGB8888,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 0,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 8,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 16,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_A,
                                             .offsetInBits = 24,
                                             .sizeInBits = 8}},
                             .sampleIncrementInBits = 32,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_NV12,
                     {{
                              .components = {{.type = android::gralloc4::PlaneLayoutComponentType_Y,
                                              .offsetInBits = 0,
                                              .sizeInBits = 8}},
                              .sampleIncrementInBits = 8,
                              .horizontalSubsampling = 1,
                              .verticalSubsampling = 1,
                      },
                      {
                              .components =
                                      {{.type = android::gralloc4::PlaneLayoutComponentType_CB,
                                        .offsetInBits = 0,
                                        .sizeInBits = 8},
                                       {.type = android::gralloc4::PlaneLayoutComponentType_CR,
                                        .offsetInBits = 8,
                                        .sizeInBits = 8}},
                              .sampleIncrementInBits = 16,
                              .horizontalSubsampling = 2,
                              .verticalSubsampling = 2,
                      }}},

                    {GBM_FORMAT_NV21,
                     {{
                              .components = {{.type = android::gralloc4::PlaneLayoutComponentType_Y,
                                              .offsetInBits = 0,
                                              .sizeInBits = 8}},
                              .sampleIncrementInBits = 8,
                              .horizontalSubsampling = 1,
                              .verticalSubsampling = 1,
                      },
                      {
                              .components =
                                      {{.type = android::gralloc4::PlaneLayoutComponentType_CR,
                                        .offsetInBits = 0,
                                        .sizeInBits = 8},
                                       {.type = android::gralloc4::PlaneLayoutComponentType_CB,
                                        .offsetInBits = 8,
                                        .sizeInBits = 8}},
                              .sampleIncrementInBits = 16,
                              .horizontalSubsampling = 2,
                              .verticalSubsampling = 2,
                      }}},

                    {GBM_FORMAT_R8,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 8}},
                             .sampleIncrementInBits = 8,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_R16,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 16}},
                             .sampleIncrementInBits = 16,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_RGB565,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 0,
                                             .sizeInBits = 5},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 5,
                                             .sizeInBits = 6},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 11,
                                             .sizeInBits = 5}},
                             .sampleIncrementInBits = 16,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_BGR888,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 8,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 16,
                                             .sizeInBits = 8}},
                             .sampleIncrementInBits = 24,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_XBGR8888,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 8,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 16,
                                             .sizeInBits = 8}},
                             .sampleIncrementInBits = 32,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_YVU420,
                     {
                             {
                                     .components = {{.type = android::gralloc4::
                                                             PlaneLayoutComponentType_Y,
                                                     .offsetInBits = 0,
                                                     .sizeInBits = 8}},
                                     .sampleIncrementInBits = 8,
                                     .horizontalSubsampling = 1,
                                     .verticalSubsampling = 1,
                             },
                             {
                                     .components = {{.type = android::gralloc4::
                                                             PlaneLayoutComponentType_CR,
                                                     .offsetInBits = 0,
                                                     .sizeInBits = 8}},
                                     .sampleIncrementInBits = 8,
                                     .horizontalSubsampling = 2,
                                     .verticalSubsampling = 2,
                             },
                             {
                                     .components = {{.type = android::gralloc4::
                                                             PlaneLayoutComponentType_CB,
                                                     .offsetInBits = 0,
                                                     .sizeInBits = 8}},
                                     .sampleIncrementInBits = 8,
                                     .horizontalSubsampling = 2,
                                     .verticalSubsampling = 2,
                             },
                     }},

                    {GBM_FORMAT_RGBX8888,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 16,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 8,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 8}},
                             .sampleIncrementInBits = 32,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    {GBM_FORMAT_XRGB8888,
                     {{
                             .components = {{.type = android::gralloc4::PlaneLayoutComponentType_B,
                                             .offsetInBits = 16,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_G,
                                             .offsetInBits = 8,
                                             .sizeInBits = 8},
                                            {.type = android::gralloc4::PlaneLayoutComponentType_R,
                                             .offsetInBits = 0,
                                             .sizeInBits = 8}},
                             .sampleIncrementInBits = 32,
                             .horizontalSubsampling = 1,
                             .verticalSubsampling = 1,
                     }}},

                    // TODO: Add support for more GBM pixel format.
            });
    return *kPlaneLayoutsMap;
}

inline int getPlaneLayouts(uint32_t gbm_format, std::vector<PlaneLayout>* outPlaneLayouts)
{
    const auto& planeLayoutsMap = GetPlaneLayoutsMap();
    const auto it = planeLayoutsMap.find(gbm_format);
    if (it == planeLayoutsMap.end()) {
        ALOGE("getPlaneLayouts failed: Unknown plane layout for format %d", gbm_format);
        return -EINVAL;
    }

    *outPlaneLayouts = it->second;
    return 0;
}

int32_t GrallocGbmMapperV5::getStandardMetadata(buffer_handle_t _Nonnull buffer, int64_t standardMetadataType,
						void* _Nonnull outData, size_t outDataSize)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("getStandardMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}
	// Convert the int64_t to StandardMetadataType enum and get readable name
	StandardMetadataType metadataTypeEnum = static_cast<StandardMetadataType>(standardMetadataType);
	std::string metadataTypeName = toString(metadataTypeEnum);

	ALOGV("get standard metadata %s", metadataTypeName.c_str());

	auto provider = [&]<StandardMetadataType T>(auto&& provide) -> int32_t {
		return grallocGbmQueryAndroidBufferMetadata(buffer, provide, StandardMetadata<T>{});
	};
    
	return provideStandardMetadata(static_cast<StandardMetadataType>(standardMetadataType), 
				       outData, outDataSize, provider);

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::setMetadata(buffer_handle_t _Nonnull buffer, AIMapper_MetadataType metadataType,
					       const void* _Nonnull metadata, size_t metadataSize)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("setMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}

	if (is_standard_metadata(metadataType))
		return setStandardMetadata(buffer, metadataType.value, metadata, metadataSize);

	ALOGE("setMetadata failed: Non-standard metadata (%s) is unsupported!", metadataType.name);
	return AIMAPPER_ERROR_UNSUPPORTED;
}

AIMapper_Error GrallocGbmMapperV5::setStandardMetadata(buffer_handle_t _Nonnull buffer,
						       int64_t standardMetadataType, const void* _Nonnull metadata,
						       size_t metadataSize)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("setStandardMetadata failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}
	// Convert the int64_t to StandardMetadataType enum and get readable name
	StandardMetadataType metadataTypeEnum = static_cast<StandardMetadataType>(standardMetadataType);
	std::string metadataTypeName = toString(metadataTypeEnum);

	ALOGV("set standard metadata %s (size: %zu)", metadataTypeName.c_str(), metadataSize);
	gbm_bo_data_t *bo_data = gralloc_gbm_android_buffer_get_extdata(buffer);
	if (!bo_data) {
		ALOGE("setStandardMetadata failed: Unable to find the extend data of buffer!");
		return AIMAPPER_ERROR_NO_RESOURCES;
	}

	switch (metadataTypeEnum) {
        case StandardMetadataType::BUFFER_ID:
        case StandardMetadataType::NAME:
        case StandardMetadataType::WIDTH:
        case StandardMetadataType::HEIGHT:
        case StandardMetadataType::LAYER_COUNT:
        case StandardMetadataType::PIXEL_FORMAT_REQUESTED:
        case StandardMetadataType::PIXEL_FORMAT_FOURCC:
        case StandardMetadataType::PIXEL_FORMAT_MODIFIER:
        case StandardMetadataType::USAGE:
        case StandardMetadataType::STRIDE:
		ALOGE("setStandardMetadata failed: Read-only metadata type (%s).", metadataTypeName.c_str());
		return AIMAPPER_ERROR_BAD_VALUE;
	case StandardMetadataType::DATASPACE:
		bo_data->dataspace = static_cast<int32_t>(*(Dataspace *)metadata);
		ALOGV("set DATASPACE to %d, received %d (%s)", bo_data->dataspace, *(Dataspace *)metadata, toString(*(Dataspace *)metadata).c_str());
		break;
	case StandardMetadataType::BLEND_MODE:
		bo_data->blend_mode = static_cast<int32_t>(*(BlendMode *)metadata);
		ALOGV("set BLEND_MODE to %d, received %d (%s)", bo_data->blend_mode, *(BlendMode *)metadata, toString(*(BlendMode *)metadata).c_str());
		break;
	case StandardMetadataType::SMPTE2086:
	case StandardMetadataType::CTA861_3:
	case StandardMetadataType::SMPTE2094_40:
	case StandardMetadataType::SMPTE2094_10:
		ALOGW("known metadata type but not implemented (%s).", metadataTypeName.c_str());
		break;
	default:
		ALOGD("unsupported metadata type (%s).", metadataTypeName.c_str());
	}

	return AIMAPPER_ERROR_NONE;
}

static constexpr std::array<AIMapper_MetadataTypeDescription, 19> sSupportedMetadataTypes {
	/* Read-only types */
	newStandardMetadata(StandardMetadataType::BUFFER_ID, true, false),
	newStandardMetadata(StandardMetadataType::NAME, false, false),
	newStandardMetadata(StandardMetadataType::WIDTH, true, false),
	newStandardMetadata(StandardMetadataType::HEIGHT, true, false),
	newStandardMetadata(StandardMetadataType::LAYER_COUNT, true, false),
	newStandardMetadata(StandardMetadataType::PIXEL_FORMAT_REQUESTED, true, false),
	newStandardMetadata(StandardMetadataType::PIXEL_FORMAT_FOURCC, true, false),
	newStandardMetadata(StandardMetadataType::PIXEL_FORMAT_MODIFIER, true, false),
	newStandardMetadata(StandardMetadataType::USAGE, true, false),
	newStandardMetadata(StandardMetadataType::ALLOCATION_SIZE, true, false),
	newStandardMetadata(StandardMetadataType::PROTECTED_CONTENT, false, false),
	newStandardMetadata(StandardMetadataType::STRIDE, true, false),
	newStandardMetadata(StandardMetadataType::COMPRESSION, true, false),
	newStandardMetadata(StandardMetadataType::INTERLACED, true, false),
	newStandardMetadata(StandardMetadataType::CHROMA_SITING, true, false),
	newStandardMetadata(StandardMetadataType::PLANE_LAYOUTS, true, false),
	newStandardMetadata(StandardMetadataType::CROP, true, false),
	/* Writable types */
	newStandardMetadata(StandardMetadataType::DATASPACE, true, true),
	newStandardMetadata(StandardMetadataType::BLEND_MODE, true, true),
};

AIMapper_Error GrallocGbmMapperV5::listSupportedMetadataTypes(const AIMapper_MetadataTypeDescription* _Nullable* _Nonnull outDescriptionList,
							      size_t* _Nonnull outNumberOfDescriptions)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	*outDescriptionList = sSupportedMetadataTypes.data();
	*outNumberOfDescriptions = sSupportedMetadataTypes.size();

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::dumpBuffer(buffer_handle_t _Nonnull bufferHandle,
					      AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
					      void* _Null_unspecified context)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!bufferHandle) {
		ALOGE("dumpBuffer failed: invalid buffer (%p).", bufferHandle);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}
	
	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("dumpBuffer failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_UNSUPPORTED;
		}
	}
	
	auto callback = [&](AIMapper_MetadataType type, const std::vector<uint8_t>& buffer) {
		dumpBufferCallback(context, type, buffer.data(), buffer.size());
	};

	return AIMAPPER_ERROR_NONE;
}

AIMapper_Error GrallocGbmMapperV5::dumpAllBuffers(AIMapper_BeginDumpBufferCallback _Nonnull beginDumpBufferCallback,
				  		  AIMapper_DumpBufferCallback _Nonnull dumpBufferCallback,
				  		  void* _Null_unspecified context)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	
	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("dumpAllBuffers failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_UNSUPPORTED;
		}
	}

	auto callback = [&](AIMapper_MetadataType type, const std::vector<uint8_t>& buffer) {
		//beginDumpBufferCallback(context);
		dumpBufferCallback(context, type, buffer.data(), buffer.size());
	};

	return AIMAPPER_ERROR_NONE;
}

// TODO: Add reserved region support.
AIMapper_Error GrallocGbmMapperV5::getReservedRegion(buffer_handle_t _Nonnull buffer,
						     void* _Nullable* _Nonnull outReservedRegion,
						     uint64_t* _Nonnull outReservedSize)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer) {
		ALOGE("getReservedRegion failed: invalid buffer (%p).", buffer);
		return AIMAPPER_ERROR_BAD_BUFFER;
	}
	
	if (!is_gralloc_gbm_ready()) {
		if(gralloc_gbm_init()) {
			ALOGE("getReservedRegion failed: Failed to initialize the gralloc_gbm driver");
			return AIMAPPER_ERROR_UNSUPPORTED;
		}
	}

	*outReservedRegion = nullptr;
	*outReservedSize = 0;
	ALOGW("We currently not support reserved region.");
	return AIMAPPER_ERROR_NONE;
}

extern "C" uint32_t ANDROID_HAL_MAPPER_VERSION = AIMAPPER_VERSION_5;

extern "C" AIMapper_Error AIMapper_loadIMapper(AIMapper* _Nullable* _Nonnull outImplementation) {
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	assert(outImplementation);

	static vendor::mapper::IMapperProvider<GrallocGbmMapperV5> provider;
	return provider.load(outImplementation);
}
