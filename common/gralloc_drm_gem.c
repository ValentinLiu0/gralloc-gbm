#include <dlfcn.h>
#include <fcntl.h>
#include <hardware/gralloc.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <sys/mman.h>

#include <drm_gem_driver.h>
#include <gralloc_drm_gem.h>
#include <gralloc_handle.h>

#define LOG_TAG "gralloc_drm_gem"
#include <private/log.h>

#define DRM_GEM_DRIVER_ABI_CURRENT DRM_GEM_DRIVER_ABI_VERSION_0_0_1

static int dri_fd = 0;
static const drm_gem_driver_ops_t *g_ops = nullptr;
static void *drv_handle;

bool is_gralloc_drm_gem_ready(void)
{
	if (dri_fd > 0 && !!g_ops)
		return true;

	return false;
}

int gralloc_drm_gem_init(void)
{
	LOG_TRACE();
	dri_fd = drmOpenRender(128);
	if (dri_fd <= 0) {
		LOG_E("Unable to open DRM render device.");
		return -EINVAL;
	}

	drmVersionPtr drm_version = drmGetVersion(dri_fd);

	char library_name[256];
	snprintf(library_name, 256, "drm_gem.%s.so", drm_version->name);
	LOG_D("Trying open library \"%s\"...", library_name);
	drv_handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL);
	if (!drv_handle) {
		LOG_E("Unable to open DRM GEM driver library.");
		return -EINVAL;
	}

	dlerror();
	const drm_gem_driver_ops_t *ops = (const drm_gem_driver_ops_t *) dlsym(drv_handle, "drm_gem_driver_ops");
	const char *err = dlerror();
	if (err || !ops) {
		LOG_E("dlsym failed: %s\n", err ? err : "symbol is NULL");
		dlclose(drv_handle);
		return -EINVAL;
	}

	if (ops->abi_version != DRM_GEM_DRIVER_ABI_CURRENT) {
		LOG_E("Mismatched ABI version! Expected %u but received %u, please update your driver.", DRM_GEM_DRIVER_ABI_CURRENT, ops->abi_version);
		dlclose(drv_handle);
		return -EINVAL;
	}

	g_ops = ops;
	LOG_D("Loaded DRM GEM driver \"%s\" (version %u)", library_name, g_ops->abi_version);

	int ret = g_ops->init(dri_fd);
	if (ret) {
		LOG_E("DRM GEM Driver initialization failed, ret=%d.", ret);
		dlclose(drv_handle);
		return ret;
	}

	return 0;
}

bool gralloc_drm_gem_is_allocator_desc_supported(const allocator_desc_t *desc)
{
	LOG_TRACE();
	switch (desc->format) {
	case HAL_PIXEL_FORMAT_BLOB:
	return true;
	default:
	return false;
	}
}

inline uint32_t calculate_drm_flags(const int usage)
{
	LOG_TRACE();
	uint32_t drm_flags = 0;
	if ((usage & GRALLOC_USAGE_SW_READ_OFTEN) || (usage & GRALLOC_USAGE_SW_READ_RARELY) ||
	    (usage & GRALLOC_USAGE_SW_WRITE_OFTEN)  || (usage & GRALLOC_USAGE_SW_WRITE_RARELY))
		drm_flags |= GRALLOC_DRM_GEM_FLAG_CPU_MMAP;
	return drm_flags;
}

inline uint8_t get_bytes_per_pixel(const int android_format)
{
	LOG_TRACE();
	uint8_t bytes = 0;
	switch (android_format) {
	case HAL_PIXEL_FORMAT_BLOB:
	bytes = 1; // 1 Pixel has 1 Byte
	default:
	bytes = 0;
	}

	return bytes;
}

inline uint32_t calculate_stride(const int android_format, const int32_t width)
{
	LOG_TRACE();
	uint8_t bpp = 0; // bits per pixel
	switch (android_format) {
	case HAL_PIXEL_FORMAT_BLOB:
	bpp = 8 * get_bytes_per_pixel(android_format);
	break;
	default:
	bpp = 0;
	}

	return width * bpp;
}

int gralloc_drm_gem_bo_create(allocator_desc_t *desc, uint32_t *out_stride, native_handle_t **out_buffer_handle)
{
	LOG_TRACE();
	if (!desc) {
		LOG_E("Invalid descriptor");
		return -EINVAL;
	}
	int ret = 0;

	uint32_t flags = calculate_drm_flags(desc->usage);

	int pfd = 0;	
	uint64_t basic_size = desc->width * desc->height;

	// TODO: Add more support
	uint64_t size = basic_size;

	ret = g_ops->bo_alloc(size, flags, &pfd);
	if (ret) {
		LOG_E("DRM operation failed, ret=%d", ret);
		return ret;
	}

	native_handle_t *handle = gralloc_handle_create(desc->width, desc->height, desc->format, desc->usage);
	if (!handle) {
		LOG_E("Failed to create gralloc handle (%ux%u), hal fmt=%d, usage=0x%lx.",
		      desc->width, desc->height, desc->format,
		      desc->usage);
		return -EINVAL;
	}

	struct gralloc_handle_t *ghandle = gralloc_handle(handle);
	if (!ghandle) {
		return -EINVAL;
	}
	ghandle->prime_fd = pfd;
	// ghandle->stride = stride / bytes per pixel
	ghandle->stride = calculate_stride(desc->format, desc->width) / get_bytes_per_pixel(desc->format);
	ghandle->modifier = 0;

	*out_buffer_handle = handle;
	if (out_stride)
		*out_stride = ghandle->stride;

	LOG_D("new buffer (fd=%u): %ux%u, hal fmt=%d, flags=0x%x, stride=%u, modifier=%lu",
	      pfd, desc->width, desc->height, desc->format,
	      flags, ghandle->stride, ghandle->modifier);
	return 0;
}

int gralloc_drm_gem_bo_destory(int prime_fd)
{
	LOG_TRACE();
	if (prime_fd <= 0) {
		LOG_E("Invalid prime_fd: %d", prime_fd);
		return -EINVAL;
	}

	return g_ops->bo_free(prime_fd);
}

/** 
 * Defined at AllocationResult.aidl
 * Return the stride in pixels
 */
int32_t gralloc_drm_gem_caculate_android_pixel_stride(const int32_t android_format, const uint32_t stride /*pitch*/)
{
	LOG_TRACE();

	return stride / get_bytes_per_pixel(android_format);
}
