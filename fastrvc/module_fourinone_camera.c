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
#include <unistd.h>
#include "module_v4l2.h"

typedef struct v4l2_4in1_camera {
	// common
	v4l2_device v4l2_base;
	// private
	unsigned int camera_index;
	unsigned int camera_count;
	unsigned int data_offset;
} v4l2_4in1_camera;

static int fourinone_camera_mem_caps(void *private, int cap, int *val)
{
	(void)private;

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

static void* fourinone_camera_mem_get_ptr(void *private, void *buff)
{
	char *ptr;
	struct v4l2_device *dev;
	struct v4l2_4in1_camera *camera;
	dev = (v4l2_device*)private;
	camera = (v4l2_4in1_camera*)private;

	struct v4l2_buffer *buf;
	buf = (struct v4l2_buffer*)buff;

	struct v4l2_buffer_info *buf_info;
	if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
		buf_info = &dev->out_buff;
	} else {
		buf_info = &dev->cap_buff;
	}

	if (buf->index >= buf_info->count)
		return NULL;

	if (camera->camera_count > 1) {
		if (V4L2_TYPE_IS_MULTIPLANAR(buf_info->buffer_type))
			return NULL;
	}
	ptr = (char*)buf_info->va[buf->index].va_list;
	return (void*)(ptr + camera->data_offset);
}

static memory_ops fourinone_camera_mem_ops = {
	.caps = fourinone_camera_mem_caps,
	.alloc = v4l2_mem_alloc,
	.free = v4l2_mem_free,
	.get_ptr = fourinone_camera_mem_get_ptr,
};

static void fourinone_camera_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct v4l2_4in1_camera *dev;
	dev = (v4l2_4in1_camera*)module_get_driver_data(mod);

	if (strcmp(name, "camera_index") == 0) {
		dev->camera_index = value;
	} else if (strcmp(name, "camera_count") == 0) {
		dev->camera_count = value;
	} else {
		v4l2_driver_config(mod, name, value);
	}
}

static int fix_camera_count(struct v4l2_device *device)
{
	int ret, count;
	count = 0;
	struct v4l2_streamparm streamparm;

	do {
		if (!device->name) {
			LOG_ERR("null device->name");
			return -1;
		}
		ret = v4l2_open(device->name, &device->fd);
		if (ret != 0) {
			LOG_VERBOSE(10, "v4l2_open error, will retry");
			usleep(10000);
		}
	} while (ret != 0);

	streamparm.type = device->cap_buff.buffer_type;
	if (v4l2_g_parm(device->fd, &streamparm) == 0) {
		while((count < 5) && streamparm.parm.capture.reserved[0]) {
			count += streamparm.parm.capture.reserved[0] & 0x1U;
			streamparm.parm.capture.reserved[0] >>= 1;
		}
	} else {
		LOG_ERR("v4l2_g_parm() Error");
		return 0;
	}

	v4l2_close(&device->fd);
	return count;
}

static int fourinone_camera_driver_init(struct module *mod)
{
	int count;
	struct v4l2_4in1_camera *dev;
	dev = (v4l2_4in1_camera*)module_get_driver_data(mod);

	count = fix_camera_count(&dev->v4l2_base);
	if (count != 0) {
		// if camera_count configured in xml, we check if count == camera_count
		if ((dev->camera_count != 0) && (dev->camera_count != count)) {
			LOG_ERR("camera count detected not equal to camera_count in xml, something wrong");
			return -1;
		} else {
			dev->camera_count = count;
		}
	}

	if (dev->camera_count == 0)
		dev->camera_count = 1;

	if (dev->camera_index >= dev->camera_count) {
		LOG_ERR("camera_index >= camera_count, please check your fastrvc_config.xml");
		return -1;
	}

	if (dev->camera_count > 1) {
		if (V4L2_TYPE_IS_MULTIPLANAR(dev->v4l2_base.cap_buff.buffer_type)) {
			LOG_ERR("multi camera cannot support multi-plane");
			return -1;
		}
		dev->v4l2_base.cap_buff.height *= dev->camera_count;
	}

	if (0 != v4l2_driver_init(mod))
		return -1;

	dev->data_offset =
		dev->v4l2_base.cap_buff.sizeimage / dev->camera_count * dev->camera_index;

	return 0;
}

static int fourinone_camera_driver_handle_frame(struct module *mod)
{
	struct shm_buff *shm;
	// 'alloc' will dequeue frame, and 'free' enqueue by next module
	shm = module_alloc_buffer(mod, BUFFER_OUTPUT, 0);
	if (shm) {
		LOG_ONCE("push first frame");
		LOG_VERBOSE(1000, "get frame");
		module_push_frame(mod, shm);
	}
	usleep(1000); // we use non-blocking mode, sleep reduce cpu loading
	return 0;
}

static module_driver fourinone_camera_driver = {
	.config = fourinone_camera_driver_config,
	.init = fourinone_camera_driver_init,
	.deinit = v4l2_driver_deinit,
	.handle_frame = fourinone_camera_driver_handle_frame,
};

int fourinone_camera_module_init(module *mod)
{
	struct v4l2_4in1_camera *dev;
	dev = (v4l2_4in1_camera*)malloc(sizeof(v4l2_4in1_camera));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "4in1_camera");
	module_register_driver(mod, &fourinone_camera_driver, dev);
	module_register_allocator(mod, &fourinone_camera_mem_ops, dev);
	return 0;
}
