/*
 * SPDX-FileCopyrightText: Copyright 2026 Valentin Liu (LIU, YUANCHEN) <valentinliu@icloud.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <drm/android/gralloc_handle.h>
#include <fcntl.h>
#include <linux/errno.h>
#include <stddef.h>
#include <stdlib.h>

#include <gralloc_gbm_format.h>
#include <gralloc_gbm.h>

#define LOG_TAG "gralloc_gbm"
#include <cutils/log.h>

/**
 * Hash table to store managed (allocated/imported) android buffer.
 */
static android_buffer_t *android_buffer_table = NULL;

/* Gralloc GBM android_buffer_t functions */

android_buffer_t *find_buffer_k(const int fd)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);

	android_buffer_t *entry;
	HASH_FIND_INT(android_buffer_table, &fd, entry);
	if (entry)
		return entry;

	return nullptr;
}

android_buffer_t *find_buffer_v(const void *v)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!v) {
		ALOGE("Invalid v: %p, return nullptr", v);
		return nullptr;
	}

	android_buffer_t *entry, *tmp;
	ALOGV("buffer table count: %d", HASH_COUNT(android_buffer_table));
	HASH_ITER(hh, android_buffer_table, entry, tmp) {
		if (entry->bo == v)
			return entry;
	}
	
	return nullptr;
}

int add_buffer(android_buffer_t *buffer)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer)
		return -EINVAL;
	
	if (find_buffer_k(buffer->fd)) {
		ALOGD("trying to add an existed buffer, skip.");
	}

	if (!buffer->bo)
		ALOGW("Trying to add a buffer with invalid value.");

	HASH_ADD_INT(android_buffer_table, fd, buffer);
	
	android_buffer_t *tmp;
	HASH_FIND_INT(android_buffer_table, &buffer->fd, tmp);
	assert(buffer->fd == tmp->fd);

	return 0;
}

int delete_buffer(android_buffer_t *buffer)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!buffer)
		return -EINVAL;

	HASH_DEL(android_buffer_table, buffer);
	free(buffer);

	return 0;
}

int delete_buffer_fd(const int fd)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (fd < 0)
		return -EINVAL;

	android_buffer_t *tmp;
	HASH_FIND_INT(android_buffer_table, &fd, tmp);
	if (fd) {
		HASH_DEL(android_buffer_table, tmp);
		free(tmp);
	} else {
		ALOGD("no fd=%d buffer found, skip deleting.", fd);
	}

	return 0;
}

static gralloc_gbm_driver_t *g_driver;

/* Gralloc GBM driver functions */

bool is_gralloc_gbm_ready(void)
{
	if (g_driver && g_driver->ready)
		return true;

	return false;
}

int gralloc_gbm_init(void)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (g_driver && g_driver->ready)
		return 0;

	g_driver = calloc(1, sizeof(*g_driver));
	if (!g_driver)
		return -ENOMEM;

	g_driver->dri_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
	if (g_driver->dri_fd < 0) {
		free(g_driver);
		return -ENODEV;
	}

	g_driver->gbm_dev = gbm_create_device(g_driver->dri_fd);
	if (!g_driver->gbm_dev) {
		close(g_driver->dri_fd);
		free(g_driver);
		return -EINVAL;
	}

	g_driver->gbm_dev_fd = gbm_device_get_fd(g_driver->gbm_dev);
	g_driver->gbm_backend_name = gbm_device_get_backend_name(g_driver->gbm_dev);
	g_driver->ready = true;

	ALOGV("initialized Gralloc GBM driver! (Backend: %s)", g_driver->gbm_backend_name);

	return 0;
}

int gralloc_gbm_deinit(void)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!g_driver->ready)
		ALOGW("Not allowed to uninitialized driver, nothing to do.");

	gbm_device_destroy(g_driver->gbm_dev);

	free(g_driver);

	return 0;
}

// Callback for gbm_bo_set_user_data()
void gralloc_gbm_bo_data_destroy(struct gbm_bo *bo, void *data) {
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) data;
	free(bo_data);

	(void)bo;
}

const gralloc_gbm_driver_t *gralloc_gbm_get(void)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready())
		return nullptr;

	return g_driver;
}

inline gbm_bo_data_t *gralloc_gbm_bo_user_data_init(struct gbm_bo *bo)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!bo)
		return nullptr;
 
	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) gbm_bo_get_user_data(bo);
	if (!bo_data) {
		ALOGV("%s: no user data found in BO (%p), set a new one.", __func__, bo);
		bo_data = calloc(1, sizeof(gbm_bo_data_t));
		gbm_bo_set_user_data(bo, bo_data, gralloc_gbm_bo_data_destroy);
	}

	return bo_data;
}

/* Gralloc GBM Buffer Object functions */

static int gralloc_gbm_bo_map(struct gbm_bo *bo, int enable_write, void **addr)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!bo)
		return -EINVAL;

	int flags = GBM_BO_TRANSFER_READ;
    	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) gbm_bo_get_user_data(bo);
    	uint32_t stride;

	if (bo_data->map_data)
		return -EINVAL;

	if (enable_write)
        	flags |= GBM_BO_TRANSFER_WRITE;

	*addr = gbm_bo_map(bo, 0, 0, gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                       flags, &stride, &bo_data->map_data);
	if (*addr == NULL)
		return -ENOMEM;

	assert(stride == gbm_bo_get_stride(bo));

	ALOGV("mapped BO (%p) at %p", bo, *addr);
	return 0;
}

static void gralloc_gbm_bo_unmap(struct gbm_bo *bo) {
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	gbm_bo_data_t *bo_data = (gbm_bo_data_t *) gbm_bo_get_user_data(bo);

	gbm_bo_unmap(bo, bo_data->map_data);
	bo_data->map_data = NULL;
	ALOGV("unmapped BO (%p).", bo);
}

/* Gralloc GBM Android platform functions */

/** 
 * Defined at AllocationResult.aidl
 * Return the stride in pixels
 */
int32_t gralloc_gbm_caculate_android_pixel_stride(const int32_t android_format, const uint32_t gbm_stride /*pitch*/)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	uint32_t gbm_format = color_fmt_a2g(android_format);
	if (gbm_format <= 0)
		return -EINVAL;

	return gbm_stride / color_bytes_per_pixel(gbm_format);
}

uint_t gralloc_gbm_caculate_gbm_flags(const int usage, const int gbm_format)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	uint_t flags = GBM_BO_USE_NONE;

	if (usage & GRALLOC_USAGE_SW_READ_OFTEN)
		flags |= GBM_BO_USE_LINEAR;
	if (usage & GRALLOC_USAGE_SW_READ_RARELY)
		flags |= GBM_BO_USE_LINEAR;
	if (usage & GRALLOC_USAGE_SW_WRITE_OFTEN)
		flags |= GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE;
	if (usage & GRALLOC_USAGE_SW_WRITE_RARELY)
		flags |= GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE;
	if (usage & GRALLOC_USAGE_HW_TEXTURE)
		flags |= GBM_BO_USE_RENDERING; // TODO: Check this.
	if (usage & GRALLOC_USAGE_HW_RENDER)
		flags |= GBM_BO_USE_RENDERING;
	if (usage & GRALLOC_USAGE_HW_2D)
		flags |= GBM_BO_USE_RENDERING;
	if (usage & GRALLOC_USAGE_HW_COMPOSER)
		flags |= GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
	if (usage & GRALLOC_USAGE_HW_FB)
		flags |= GBM_BO_USE_SCANOUT;
	if (usage & GRALLOC_USAGE_EXTERNAL_DISP)
		flags |= GBM_BO_USE_NONE;
	if (usage & GRALLOC_USAGE_PROTECTED)
		flags |= GBM_BO_USE_PROTECTED;
	if (usage & GRALLOC_USAGE_CURSOR)
		flags |= GBM_BO_USE_CURSOR;
	//if (usage & GRALLOC_USAGE_RENDERSCRIPT)
	//	flags |= GBM_BO_USE_FRONT_RENDERING;

	if ((flags & GBM_BO_USE_SCANOUT) &&
		!(gbm_format == GBM_FORMAT_XRGB8888 || gbm_format == GBM_FORMAT_XBGR8888)) {
		ALOGW("We added GBM_BO_USE_SCANOUT but using unsupported format (%s).\n" \
		      "This may cause an issue if the flags has GBM_BO_USE_WRITE.", color_fmt_getname(gbm_format, true));
	}

	ALOGV_IF((flags == GBM_BO_USE_NONE), "No flag matched.");

	return flags;
}

bool gralloc_gbm_is_allocator_desc_supported(const allocator_desc_t *desc)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready())
		return false;

	if (!desc)
		return false;

	uint32_t gbm_format = color_fmt_a2g(desc->format);
	if (gbm_format <= 0)
		return false;

	if ((desc->width > GRALLOC_GBM_WIDTH_MAX) || (desc->height > GRALLOC_GBM_HEIGHT_MAX))
		return false;

	if (!desc->usage)
		return false;

	uint32_t flags = gralloc_gbm_caculate_gbm_flags(desc->usage, gbm_format);

	if (!gbm_device_is_format_supported(g_driver->gbm_dev, gbm_format, flags))
		return false;

	return true;
}

int gralloc_gbm_android_buffer_new(allocator_desc_t *desc, __nullable uint32_t *out_stride, native_handle_t **out_buffer_handle)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready())
		return -ENODEV;

	if (!gralloc_gbm_is_allocator_desc_supported(desc))
		return -EINVAL;

	uint32_t gbm_format = color_fmt_a2g(desc->format);
	if (gbm_format <= 0)
		return -EINVAL;

	uint_t flags = gralloc_gbm_caculate_gbm_flags(desc->usage, gbm_format);

	int32_t width = desc->width;
	int32_t height = desc->height;
	if (flags & GBM_BO_USE_CURSOR) {
		width = ALIGN(MAX(desc->width, 64), 16);
		height = ALIGN(MAX(desc->height, 64), 16);
	}
	/*
	 * For YV12, we request GR88, so halve the width since we're getting
	 * 16bpp. Then increase the height by 1.5 for the U and V planes.
	 */
	if (desc->format == HAL_PIXEL_FORMAT_YV12) {
		width = ALIGN(desc->width, 32) / 2;
		height += ALIGN(desc->height, 2) / 2;
		ALOGD("width and height changed to %dx%d for HAL_PIXEL_FORMAT_YV12.", width, height);
	}

	native_handle_t *handle = gralloc_handle_create(width, height, desc->format, desc->usage);
	if (!handle)
		return -EINVAL;

	struct gbm_bo *bo = gbm_bo_create(g_driver->gbm_dev, width, height, gbm_format, flags);
	if (!bo)
		return -EINVAL;

	gralloc_gbm_bo_user_data_init(bo);

	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		return -EINVAL;
	}
	ghandle->stride = gbm_bo_get_stride(bo);
	ghandle->prime_fd = gbm_bo_get_fd(bo);
	ghandle->modifier = gbm_bo_get_modifier(bo);

	android_buffer_t *buffer = calloc(1, sizeof(android_buffer_t));
	if (!buffer) {
		return -ENOMEM;
	}
	buffer->fd = ghandle->prime_fd;
	buffer->bo = bo;
	add_buffer(buffer);

	*out_buffer_handle = handle;
	if (out_stride)
		*out_stride = ghandle->stride;

	ALOGD("new buffer object (%p, fd=%u): %ux%u, fmt=%s, flags=%u, stride=%u, modifier=%lu",
	      bo, gbm_bo_get_fd(bo), width, height, color_fmt_getname(gbm_format, true),
	      flags, ghandle->stride, ghandle->modifier);
	return 0;
}

int gralloc_gbm_android_buffer_import(const buffer_handle_t handle)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready())
		return -ENODEV;

	struct gbm_bo *bo;
	const struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	struct gbm_import_fd_modifier_data *fd_data = calloc(1, sizeof(struct gbm_import_fd_modifier_data));
	if (!fd_data)
		return -ENOMEM;

	fd_data->format = color_fmt_a2g(ghandle->format);
	if (fd_data->format <= 0) {
		ALOGE("Trying to import buffer with unsupported Android color format (%d), abort.",
		      ghandle->format);
		free(fd_data);
		return -EINVAL;
	}

	fd_data->width = ghandle->width;
	fd_data->height = ghandle->height;
	/* Adjust the width and height for a GBM GR88 buffer */
	if (ghandle->format == HAL_PIXEL_FORMAT_YV12) {
		fd_data->width = ALIGN(ghandle->width, 32) / 2;
		fd_data->height = ghandle->height + ALIGN(ghandle->height, 2) / 2;
	}

	/* Some GPUs require 64 pixels at least for cursor */
	if (ghandle->usage & GRALLOC_USAGE_CURSOR) {
		fd_data->width = ALIGN(MAX(ghandle->width, 64), 16);
		fd_data->height = ALIGN(MAX(ghandle->height, 64), 16);
	}

	fd_data->modifier = ghandle->modifier;
	fd_data->num_fds = 1;
	fd_data->fds[0] = ghandle->prime_fd;
	fd_data->strides[0] = ghandle->stride;

	uint32_t flags = gralloc_gbm_caculate_gbm_flags(ghandle->usage, fd_data->format);
	bo = gbm_bo_import(g_driver->gbm_dev, GBM_BO_IMPORT_FD_MODIFIER, fd_data, flags);
	if (!bo) {
		ALOGE("Failed to import buffer object (%ux%u), fmt=%s, flags=%u.",
		      fd_data->width, fd_data->height, color_fmt_getname(fd_data->format, true),
		      flags);
		free(fd_data);
		return -EINVAL;
	}

	gralloc_gbm_bo_user_data_init(bo);

	android_buffer_t *buffer = (android_buffer_t *) malloc(sizeof(android_buffer_t));
	buffer->fd = ghandle->prime_fd;
	buffer->bo = bo;
	add_buffer(buffer);

	free(fd_data);
	ALOGD("imported buffer object (%p, fd=%u): %ux%u, fmt=%s, flags=%u, stride=%u",
	      bo, fd_data->fds[0], fd_data->width, fd_data->height,
	      color_fmt_getname(fd_data->format, true), flags, fd_data->strides[0]);
	return 0;
}

int gralloc_gbm_android_buffer_free(const buffer_handle_t handle)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	assert(ghandle);

	if (!is_gralloc_gbm_ready())
		return -ENODEV;

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (buffer) {
		struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
		gbm_bo_destroy(bo);
		delete_buffer(buffer);
		ALOGV("freed buffer %p", buffer);
	}

	return 0;
}

int gralloc_gbm_android_buffer_lock(const buffer_handle_t handle, int usage,
				    const int top, const int bottom, const int left, const int right,
				    void **out)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	if (!is_gralloc_gbm_ready())
		return -ENODEV;

	/* Framebuffer must NOT be locked */
	if (usage & GRALLOC_USAGE_HW_FB) {
		ALOGW("The framebuffer shouldn't be locked!");
		return -EINVAL;
	}

	const int width = right - left;
	const int height = bottom - top;
	const struct gralloc_handle_t *ghandle = (const struct gralloc_handle_t *) (uintptr_t)handle;
	/* Protect the GPU FB/TEXTURE memory from accessing by CPU side */
	if ((ghandle->usage & usage) != (uint32_t)usage) {
		/* make FB special for testing software renderer with */
		if (!(ghandle->usage & (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN)) &&
		    !(ghandle->usage & GRALLOC_USAGE_HW_FB) &&
		    !(ghandle->usage & GRALLOC_USAGE_HW_TEXTURE)) {
			ALOGE("Unsupported buffer usage: %d (%d)!", ghandle->usage, usage);
			return -EINVAL;
		}
	}

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		ALOGV("not a managed buffer (%p, fd=%u)!", handle, ghandle->prime_fd);
		return -EINVAL;
	}

	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	gbm_bo_data_t *bo_data = gralloc_gbm_bo_user_data_init(bo);

	/* allow multiple locks with compatible usages */
	if (bo_data->lock_count && (bo_data->locked_for & usage) != usage)
		return -EINVAL;

	usage |= bo_data->locked_for;

	/* Map CPU accessible BO */
	if (usage & (GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_SW_READ_MASK)) {
		/* the driver is supposed to wait for the bo */
		int write = !!(usage & GRALLOC_USAGE_SW_WRITE_MASK);
		int err = gralloc_gbm_bo_map(bo, write, out);
		if (err)
			return err;
	} else {
	    /* kernel handles the synchronization here */
	}

	bo_data->lock_count++;
	bo_data->locked_for |= usage;

	return 0;
}

int gralloc_gbm_android_buffer_unlock(const buffer_handle_t handle)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	assert(ghandle);

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		ALOGV("not a managed buffer (%p, fd=%u)!", handle, ghandle->prime_fd);
		return -EINVAL;
	}

	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	if (!bo)
		return -EINVAL;

	gbm_bo_data_t *bo_data;
	bo_data = (gbm_bo_data_t *)gbm_bo_get_user_data(bo);

    	int mapped = bo_data->locked_for & (GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_SW_READ_MASK);

	if (!bo_data->lock_count) {
		ALOGW("Trying unlock an already unlocked BO, noting to do.");
		return 0;
	}

	if (mapped)
		gralloc_gbm_bo_unmap(bo);

	bo_data->lock_count--;
	if (!bo_data->lock_count)
		bo_data->locked_for = 0;

	return 0;
}

int gralloc_gbm_android_buffer_query(const buffer_handle_t handle, android_buffer_info_t *out)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	assert(ghandle);

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		ALOGV("not a managed buffer (%p, fd=%u)!", handle, ghandle->prime_fd);
		return -EINVAL;
	}
	
	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	if (!bo)
		return -EINVAL;

	android_buffer_info_t info = {0};
	info.fd = gbm_bo_get_fd(bo);
	info.buffer_id = buffer;
	info.width = gbm_bo_get_width(bo);
	info.height = gbm_bo_get_height(bo);
	info.layer_count = 1;
	info.plane_count = gbm_bo_get_plane_count(bo);
	info.gbm_format = gbm_bo_get_format(bo);
	info.modifier = gbm_bo_get_modifier(bo);

	gbm_bo_data_t *bo_data = gralloc_gbm_bo_user_data_init(bo);
	info.dataspace = bo_data->dataspace;
	info.blend_mode = bo_data->blend_mode;

	info.android_format = ghandle->format;
	info.usage = ghandle->usage;
	if (info.usage & GRALLOC_USAGE_PROTECTED)
		info.is_protected = true;

	info.size = gbm_bo_get_stride(bo) * info.height;
	info.stride = gralloc_gbm_caculate_android_pixel_stride(ghandle->format, gbm_bo_get_stride(bo));

	*out = info;

	return 0;
}

gbm_bo_data_t *gralloc_gbm_android_buffer_get_extdata(const buffer_handle_t handle)
{
	ALOGV("%s:%d %s", __FILE_NAME__, __LINE__, __FUNCTION__);
	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	assert(ghandle);

	android_buffer_t *buffer = find_buffer_k(ghandle->prime_fd);
	if (!buffer) {
		ALOGV("not a managed buffer (%p, fd=%u)!", handle, gralloc_handle(handle)->prime_fd);
		return nullptr;
	}
	
	struct gbm_bo *bo = (struct gbm_bo *) buffer->bo;
	if (!bo)
		return nullptr;

	return gralloc_gbm_bo_user_data_init(bo);
}
