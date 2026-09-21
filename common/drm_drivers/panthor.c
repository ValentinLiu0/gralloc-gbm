#include <drm-uapi/panthor_drm.h>

#include <drm_gem_driver.h>
#include <errno.h>

static int dri_fd = 0;

const char *panthor_drv_get_name(void)
{
	return "panthor";
}

int panthor_drv_init(int fd)
{
	dri_fd = fd;
	return 0;
}

int panthor_convert_flags(uint32_t drm_flags)
{
	uint32_t f = 0;
	if (drm_flags & GRALLOC_DRM_GEM_FLAG_CPU_MMAP)
		f |= DRM_PANTHOR_BO_NO_MMAP;
	else
	 	f |= DRM_PANTHOR_BO_WB_MMAP;
	return f;
}

int panthor_bo_alloc(uint64_t size, uint32_t drm_flags, int *out_pfd)
{
	int ret = 0;
	struct drm_panthor_bo_create bo = {
		.size = size,
		.flags = panthor_convert_flags(drm_flags),
		.exclusive_vm_id = 0,
	};

	ret = drmIoctl(dri_fd, DRM_IOCTL_PANTHOR_BO_CREATE, &bo);
	if (ret) {
		fprintf(stderr, "drmIoctl failed: %s (errno=%d)\n", strerror(errno), errno);
		return -errno;
	}

	if (out_pfd) {
		ret = drmPrimeHandleToFD(dri_fd, bo.handle, DRM_CLOEXEC | DRM_RDWR, out_pfd);
		if (ret) {
			fprintf(stderr, "drmPrimeHandleToFD failed: %s (errno=%d)\n", strerror(errno), errno);
			return -errno;
		}
	}

	ret = drmCloseBufferHandle(dri_fd, bo.handle);
	return 0;
}

int panthor_bo_free(int prime_fd)
{
	int ret = 0;
	uint32_t bo_handle;
	ret = drmPrimeFDToHandle(dri_fd, prime_fd, &bo_handle);
	if (ret) {
		fprintf(stderr, "drmPrimeFDToHandle failed: %s (errno=%d)\n", strerror(errno), errno);
		return -errno;
	}

	return 0;
}

const drm_gem_driver_ops_t GRALLOC_DRM_GEM_DRIVER_OPS = {
    .abi_version	=	DRM_GEM_DRIVER_ABI_VERSION_0_0_1,
    .get_name		=	panthor_drv_get_name,
    .init		=	panthor_drv_init,
    .bo_alloc		=	panthor_bo_alloc,
    .bo_free		=	panthor_bo_free,
};