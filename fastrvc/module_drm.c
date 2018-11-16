/*
* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein is
* confidential and proprietary to MediaTek Inc. and/or its licensors. Without
* the prior written permission of MediaTek inc. and/or its licensors, any
* reproduction, modification, use or disclosure of MediaTek Software, and
* information contained herein, in whole or in part, shall be strictly
* prohibited.
*
* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
* ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
* WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
* NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
* RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
* INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
* TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
* RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
* OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
* SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
* RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
* ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
* RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
* MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
* CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "drm_display.h"
#include "module.h"

typedef struct drm_device {
	char	*name;
	struct mtk_display disp;

	unsigned int plane_number;
	struct mtk_plane_status plane_status;

	unsigned int crtc_idx;
	unsigned int rsl_width, rsl_height;

	unsigned int fourcc;
	unsigned int buffer_count;
	unsigned int *buffer_status;
	struct raw_texture *texture;
	struct shm_buff *last_frame;
	struct shm_buff *current_frame;
	MUTEX_HANDLE(mutex_buffer);

	drmEventContext event_ctx;
	bool feed_frame;
	bool null_pause;
} drm_device;
/* flags for 'buffer_status' */
#define DRM_BUFF_FREE 0
#define DRM_BUFF_BUSY 1

/*******************************module driver**************************/

static int drm_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_SELF;
			break;
		default:
			return -1;
	}
	return 0;
}

static void* drm_mem_alloc(void *private, int type, int *size)
{
	(void)size;
	int i;

	struct drm_device *dev;
	dev = (drm_device*)private;

	if (type == BUFFER_INPUT) {
	} else {
		return NULL;
	}

	MUTEX_LOCK(dev->mutex_buffer);
	for (i = 0; i < dev->buffer_count; i++) {
		if (dev->buffer_status[i] == DRM_BUFF_FREE) {
			dev->buffer_status[i] = DRM_BUFF_BUSY;
			MUTEX_UNLOCK(dev->mutex_buffer);
			*size = dev->texture[i].size;
			return &dev->texture[i];
		}
	}
	MUTEX_UNLOCK(dev->mutex_buffer);

	return NULL;
}

static int drm_mem_free(void *private, void *buff)
{
	int i;

	struct drm_device *dev;
	dev= (drm_device*)private;

	MUTEX_LOCK(dev->mutex_buffer);
	for (i = 0; i < dev->buffer_count; i++) {
		if (&dev->texture[i] == buff) {
			dev->buffer_status[i] = DRM_BUFF_FREE;
			break;
		}
	}
	MUTEX_UNLOCK(dev->mutex_buffer);

	return 0;
}

static void* drm_mem_get_ptr(void *private, void *buff)
{
	return ((struct raw_texture *)buff)->texbuf;
}

static memory_ops drm_mem_ops = {
	.caps = drm_mem_caps,
	.alloc = drm_mem_alloc,
	.free = drm_mem_free,
	.get_ptr = drm_mem_get_ptr,
};

static void drm_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct drm_device *dev;
	dev = (drm_device*)module_get_driver_data(mod);

	if (strcmp(name, "name") == 0) {
		dev->name = strdup((char*)value);
	} else if (strcmp(name, "src_w") == 0) {
		dev->plane_status.src_w = value;
	} else if (strcmp(name, "src_h") == 0) {
		dev->plane_status.src_h = value;
	} else if (strcmp(name, "dst_w") == 0) {
		dev->plane_status.dst_w = value;
	} else if (strcmp(name, "dst_h") == 0) {
		dev->plane_status.dst_h = value;
	} else if (strcmp(name, "dst_x") == 0) {
		dev->plane_status.dst_x = value;
	} else if (strcmp(name, "dst_y") == 0) {
		dev->plane_status.dst_y = value;
	} else if (strcmp(name, "plane_number") == 0) {
		dev->plane_number = value;
	} else if (strcmp(name, "fourcc") == 0) {
		dev->fourcc = value;
	} else if (strcmp(name, "buffer_count") == 0) {
		dev->buffer_count = value;
	} else if (strcmp(name, "null_pause") == 0) {
		dev->null_pause = value;
	} else if (strcmp(name, "crtc_idx") == 0) {
		dev->crtc_idx = value;
	} else if (strcmp(name, "rsl_width") == 0) {
		dev->rsl_width = value;
	} else if (strcmp(name, "rsl_height") == 0) {
		dev->rsl_height = value;
	}
}

static int drm_create_buffer(drm_device *dev)
{
	int i;
	int ret;
	if (dev->buffer_count < 3)
		dev->buffer_count = 3;
	dev->texture = (struct raw_texture*)
						malloc(sizeof(struct raw_texture) * dev->buffer_count);
	if (!dev->texture)
		return -1;
	memset(dev->texture, 0, sizeof(struct raw_texture) * dev->buffer_count);

	dev->buffer_status = (unsigned int*)malloc(sizeof(unsigned int) * dev->buffer_count);
	memset(dev->buffer_status, 0, sizeof(unsigned int) * dev->buffer_count);

	for (i = 0; i < dev->buffer_count; i++) {
		ret = drm_alloc_gem(dev->disp.fd,
						dev->plane_status.src_w, dev->plane_status.src_h,
							dev->fourcc, &dev->texture[i]);
		if (ret != 0)
			return -1;

		ret = drm_buffer_prepare(&dev->disp, &dev->texture[i]);
		if (ret != 0)
			return -1;
	}
	return 0;
}

static void drm_destroy_buffer(drm_device *dev)
{
	int i;
	for (i = 0; i < dev->buffer_count; i++) {
		drm_buffer_release(&dev->disp, &dev->texture[i]);
		drm_free_gem(dev->disp.fd, &dev->texture[i]);
	}
	free(dev->texture);
	free(dev->buffer_status);
}
/*
static int drm_set_plane_list(struct mtk_display *disp, void *user_data, int count,
									struct raw_texture **plane_list, int *number_list)
{
	int i;
	struct raw_texture *planes[4] = {0};

	if (count < 1 || count > 4) {
		LOG_ERR("wrong planes");
		return -1;
	}

	for (i = 0; i < count; i++) {
		if (number_list[i] > 3) {
			LOG_ERR("wrong plane number");
			return -1;
		}
		planes[number_list[i]] = plane_list[i];
	}
	drm_set_plane(disp, user_data, planes[0], planes[1], planes[2], planes[3]);
	return 0;
}
*/
static int drm_set_plane_n(struct mtk_display *disp, void *user_data,
								struct raw_texture *data_plane, int number)
{
	int ret;

	switch (number) {
		case 0:
			ret = drm_set_plane(disp, user_data, data_plane, NULL, NULL, NULL);
			break;
		case 1:
			ret = drm_set_plane(disp, user_data, NULL, data_plane, NULL, NULL);
			break;
		case 2:
			ret = drm_set_plane(disp, user_data, NULL, NULL, data_plane, NULL);
			break;
		case 3:
			ret = drm_set_plane(disp, user_data, NULL, NULL, NULL, data_plane);
			break;
		default:
			LOG_ERR("drm_set_plane wrong plane number %d", number);
			ret = -1;
	}
	return ret;
}

static void drm_page_flip_handler(int fd, unsigned int frame,
          unsigned int sec, unsigned int usec, void *data)
{
	LOG_VERBOSE(1000, "Enter");
	module *mod;
	struct drm_device *dev;

	mod = (module*)data;
	dev = (drm_device*)module_get_driver_data(mod);
	dev->feed_frame = true;

	LOG_VERBOSE(1000, "Leave");
}

static void drm_handle_vsync(struct module *mod)
{
	int ret;
	fd_set fds;
	struct timeval timeout;
	struct drm_device *dev;
	dev = (drm_device*)module_get_driver_data(mod);

	FD_ZERO(&fds);
	FD_SET(dev->disp.fd, &fds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;

	ret = select(dev->disp.fd + 1, &fds, NULL, NULL, &timeout);
	if (ret > 0) {
		drmHandleEvent(dev->disp.fd, &dev->event_ctx);
	} else {
		MOD_ERR(mod, "select timeout");
		dev->feed_frame = true; // maybe someone(like weston) flushed out ours buffer, try to commit new one
	}
}

static MUTEX_HANDLE(mutex_init);
static struct mtk_display disp;
static int refer_count = 0;
static int drm_inited = 0;

static int drm_init_once(module *mod)
{
	int ret = -1;
	struct drm_device *dev;
	dev = (drm_device*)module_get_driver_data(mod);

	MUTEX_LOCK(mutex_init);
	if (drm_inited == 0) {
		disp.crtc_idx = dev->crtc_idx;
		disp.rsl_width = dev->rsl_width;
		disp.rsl_height = dev->rsl_height;

		ret = drm_init(&disp);
		if (ret != 0) {
			MOD_ERR(mod, "drm_init error");
			goto err;
		}
		ret = drm_set_mode(&disp);
		if (ret != 0) {
			MOD_ERR(mod, "drm_set_mode error");
			drm_deinit(&disp);
			goto err;
		}
		memcpy(&dev->disp, &disp, sizeof(struct mtk_display));
		drm_inited = 1;
	} else {
		memcpy(&dev->disp, &disp, sizeof(struct mtk_display));
		ret = 0;
	}

	refer_count++;
err:
	MUTEX_UNLOCK(mutex_init);
	return ret;
}

static int drm_driver_init(struct module *mod)
{
	int ret;
	struct drm_device *dev;
	dev = (drm_device*)module_get_driver_data(mod);

	MUTEX_INIT(dev->mutex_buffer);

	if (drm_init_once(mod) != 0)
		return -1;

	ret = drm_create_buffer(dev);
	if (ret != 0) {
		MOD_ERR(mod, "drm_create_buffer error");
		goto err;
	}

	memset(&dev->event_ctx, 0, sizeof(dev->event_ctx));
	dev->event_ctx.version = DRM_EVENT_CONTEXT_VERSION;
	dev->event_ctx.vblank_handler = NULL;
	dev->event_ctx.page_flip_handler = drm_page_flip_handler;

	dev->feed_frame = true;
	return 0;
err:
	drm_deinit(&dev->disp);
	return -1;
}

static void drm_driver_deinit(struct module *mod)
{
	LOG_DBG("Enter");
	struct drm_device *dev;
	dev = (drm_device*)module_get_driver_data(mod);

	if (dev->last_frame) {
		shm_free(dev->last_frame);
		dev->last_frame = NULL;
	}
	if (dev->current_frame) {
		shm_free(dev->current_frame);
		dev->current_frame = NULL;
	}

	drm_destroy_buffer(dev);
	MUTEX_LOCK(mutex_init);
	refer_count--;
	if (refer_count == 0)
		drm_deinit(&dev->disp);
	drm_inited = 0;
	MUTEX_UNLOCK(mutex_init);
	LOG_DBG("Leave");
}

static int drm_driver_handle_frame(struct module *mod)
{
	LOG_VERBOSE(10000, "Enter");
	struct shm_buff *in_frame;
	struct drm_device *dev;
	dev = (drm_device*)module_get_driver_data(mod);

	if (!dev->feed_frame) /* detect vsync */ {
		drm_handle_vsync(mod);
	} else /* push new frame */ {
		in_frame = module_get_frame(mod);
		if (in_frame) {
			LOG_ONCE("display first frame");
			dev->feed_frame = false;
			drm_set_plane_status(&dev->disp, &dev->plane_status, dev->plane_number);

			if (module_if_ours_buffer(mod, in_frame)) {
				drm_set_plane_n(&dev->disp, mod, (struct raw_texture*)in_frame->buff, dev->plane_number);

				if (dev->last_frame)
					shm_free(dev->last_frame);
				dev->last_frame = dev->current_frame;
				dev->current_frame = in_frame;
			} else {
				struct shm_buff *out_frame;

				out_frame = module_alloc_buffer(mod, BUFFER_INPUT, in_frame->size);
				memcpy(shm_get_ptr(out_frame), shm_get_ptr(in_frame), out_frame->size);
				drm_set_plane_n(&dev->disp, mod, (struct raw_texture*)out_frame->buff, dev->plane_number);
				shm_free(in_frame);

				if (dev->last_frame)
					shm_free(dev->last_frame);
				dev->last_frame = dev->current_frame;
				dev->current_frame = out_frame;
			}
			LOG_FPS("%s display", module_get_name(mod));
		}
	}

	LOG_VERBOSE(10000, "Leave");
	return 0;
}

int drm_driver_pause(module *mod)
{
	LOG_INFO("Enter");
	struct drm_device *dev;
	struct raw_texture data_init = {0};
	dev = (drm_device*)module_get_driver_data(mod);

	// handle previous event first
	if (!dev->feed_frame)
		drm_handle_vsync(mod);

	if (dev->null_pause)
		return 0;

	drm_set_plane_status(&dev->disp, &dev->plane_status, dev->plane_number);
	drm_set_plane_n(&dev->disp, mod, &data_init, dev->plane_number);
	drm_handle_vsync(mod);

	if (dev->last_frame) {
		shm_free(dev->last_frame);
		dev->last_frame = NULL;
	}
	if (dev->current_frame) {
		shm_free(dev->current_frame);
		dev->current_frame = NULL;
	}
	LOG_INFO("Leave");
	return 0;
}

module_driver drm_driver = {
	.config = drm_driver_config,
	.init = drm_driver_init,
	.deinit = drm_driver_deinit,
	.handle_frame = drm_driver_handle_frame,
	.pause = drm_driver_pause,
};

int drm_module_init(module *mod)
{
	struct drm_device *dev;
	static int init_once = 0;
	dev = (drm_device*)malloc(sizeof(drm_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "drm");
	module_register_driver(mod, &drm_driver, dev);
	module_register_allocator(mod, &drm_mem_ops, dev);

	if (init_once == 0) {
		MUTEX_INIT(mutex_init);
		init_once = 1;
	}
	return 0;
}
