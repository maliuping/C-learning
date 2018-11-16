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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "module.h"
#include "module_v4l2.h"

typedef struct va2list_device {
	char	*ptr;

	struct v4l2_plane_pix_format plane_info[VIDEO_MAX_PLANES];
	unsigned int pix_format;
	unsigned int width;
	unsigned int height;
	unsigned int num_planes;
	unsigned int length;
} va2list_device;

typedef struct va2list_buffer {
	struct shm_buff *shm;
	void **va_list;
} va2list_buffer;

/*******************************module driver**************************/

static int va2list_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
			*val = MEMORY_ALLOC_OTHERS;
			break;
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_SELF;
			break;
		default:
			return -1;
	}
	return 0;
}

static void* va2list_mem_alloc(void *private, int type, int *size)
{
	int i;
	struct shm_buff *shm;
	struct module *mod;
	struct va2list_device *dev;
	struct va2list_buffer *buffer;
	struct v4l2_plane_pix_format *plane_info;

	mod = (module*)private;
	dev = (va2list_device*)module_get_driver_data(mod);

	if (type == BUFFER_INPUT) {
		return NULL;
	} else {
	}

	shm = module_get_frame(mod); // single to multi, alloc from previous module
	if (!shm)
		return NULL;
	*size = shm->size;

	buffer = (va2list_buffer*)malloc(sizeof(va2list_buffer));
	buffer->shm = shm;
	buffer->va_list = (void*)malloc(sizeof(void*) * dev->num_planes);
	buffer->va_list[0] = shm_get_ptr(buffer->shm);

	plane_info = dev->plane_info;
	for (i = 1; i < dev->num_planes; i++) {
		buffer->va_list[i] = buffer->va_list[i-1] + plane_info[i-1].sizeimage;
	}

	return buffer;
}

static int va2list_mem_free(void *private, void *buff)
{
	struct shm_buff *shm;
	struct va2list_buffer *buffer;

	buffer = (struct va2list_buffer*)buff;

	if (buffer->va_list) {
		free(buffer->va_list);
		buffer->va_list = NULL;
	}
	shm = buffer->shm;
	free(buffer);

	shm_free(shm);
	return 0;
}

static void* va2list_mem_get_ptr(void *private, void *buff)
{
	struct va2list_buffer *buffer;
	buffer = (struct va2list_buffer*)buff;
	return (void*)buffer->va_list;
}

static memory_ops va2list_mem_ops = {
	.caps = va2list_mem_caps,
	.alloc = va2list_mem_alloc,
	.free = va2list_mem_free,
	.get_ptr = va2list_mem_get_ptr,
};

static void va2list_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct va2list_device *dev;
	dev = (va2list_device*)module_get_driver_data(mod);

	if (strcmp(name, "pix_format") == 0) {
		dev->pix_format = value;
	} else if (strcmp(name, "width") == 0) {
		dev->width = value;
	} else if (strcmp(name, "height") == 0) {
		dev->height = value;
	}
}

static int va2list_driver_init(struct module *mod)
{
	int i;
	struct va2list_device *dev;
	dev = (va2list_device*)module_get_driver_data(mod);

	v4l2_fix_planefmt(dev->pix_format, dev->width, dev->height,
						dev->plane_info, &dev->num_planes, 1);

	dev->length = 0;
	for (i = 0; i < dev->num_planes; i++) {
		dev->length += dev->plane_info[i].sizeimage;
	}

	return 0;
}

static void va2list_driver_deinit(struct module *mod)
{
	//struct va2list_device *dev;
	//dev = (va2list_device*)module_get_driver_data(mod);
}

static int va2list_driver_handle_frame(struct module *mod)
{
	struct va2list_device *dev;
	struct shm_buff *out_frame;
	dev = (va2list_device*)module_get_driver_data(mod);

	// alloc will call 'module_get_frame'
	out_frame = module_alloc_buffer(mod, BUFFER_OUTPUT, dev->length);
	if (out_frame) {
		module_push_frame(mod, out_frame);
	}
	usleep(1000);

	return 0;
}

module_driver va2list_driver = {
	.config = va2list_driver_config,
	.init = va2list_driver_init,
	.deinit = va2list_driver_deinit,
	.handle_frame = va2list_driver_handle_frame,
};

int va2list_module_init(struct module *mod)
{
	struct va2list_device *dev;
	dev = (va2list_device*)malloc(sizeof(va2list_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "va2list");
	module_register_driver(mod, &va2list_driver, dev);
	module_register_allocator(mod, &va2list_mem_ops, mod);
	return 0;
}
