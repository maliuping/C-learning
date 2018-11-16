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

typedef struct list2va_device {
	char	*ptr;

	struct v4l2_plane_pix_format plane_info[VIDEO_MAX_PLANES];
	unsigned int pix_format;
	unsigned int width;
	unsigned int height;
	unsigned int num_planes;
	unsigned int length;
} list2va_device;

typedef struct list2va_buffer {
	struct shm_buff *shm;
	void **va_list;
} list2va_buffer;

/*******************************module driver**************************/

static int list2va_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
			*val = MEMORY_ALLOC_SELF;
			break;
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_OTHERS;
			break;
		default:
			return -1;
	}
	return 0;
}

static void* list2va_mem_alloc(void *private, int type, int *size)
{
	int i;
	struct shm_buff *shm;
	struct module *mod;
	struct list2va_device *dev;
	struct list2va_buffer *buffer;
	struct v4l2_plane_pix_format *plane_info;

	mod = (module*)private;
	dev = (list2va_device*)module_get_driver_data(mod);

	if (type == BUFFER_OUTPUT) {
		return NULL;
	} else {
	}

	shm = module_next_alloc_buffer(mod, dev->length); // multi to single, alloc from next module
	if (!shm)
		return NULL;
	*size = shm->size;

	buffer = (list2va_buffer*)malloc(sizeof(list2va_buffer));
	buffer->shm = shm;
	buffer->va_list = (void*)malloc(sizeof(void*) * dev->num_planes);
	buffer->va_list[0] = shm_get_ptr(buffer->shm);

	plane_info = dev->plane_info;
	for (i = 1; i < dev->num_planes; i++) {
		buffer->va_list[i] = buffer->va_list[i-1] + plane_info[i-1].sizeimage;
	}

	return buffer;
}

static int list2va_mem_free(void *private, void *buff)
{
	struct shm_buff *shm;
	struct list2va_buffer *buffer;

	buffer = (struct list2va_buffer*)buff;

	if (buffer->va_list) {
		free(buffer->va_list);
		buffer->va_list = NULL;
	}
	shm = buffer->shm;
	free(buffer);

	if (shm)
		shm_free(shm);
	return 0;
}

static void* list2va_mem_get_ptr(void *private, void *buff)
{
	struct list2va_buffer *buffer;
	buffer = (struct list2va_buffer*)buff;
	return (void*)buffer->va_list;
}

static memory_ops list2va_mem_ops = {
	.caps = list2va_mem_caps,
	.alloc = list2va_mem_alloc,
	.free = list2va_mem_free,
	.get_ptr = list2va_mem_get_ptr,
};

static void list2va_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct list2va_device *dev;
	dev = (list2va_device*)module_get_driver_data(mod);

	if (strcmp(name, "pix_format") == 0) {
		dev->pix_format = value;
	} else if (strcmp(name, "width") == 0) {
		dev->width = value;
	} else if (strcmp(name, "height") == 0) {
		dev->height = value;
	}
}

static int list2va_driver_init(struct module *mod)
{
	int i;
	struct list2va_device *dev;
	dev = (list2va_device*)module_get_driver_data(mod);

	v4l2_fix_planefmt(dev->pix_format, dev->width, dev->height,
						dev->plane_info, &dev->num_planes, 1);

	dev->length = 0;
	for (i = 0; i < dev->num_planes; i++) {
		dev->length += dev->plane_info[i].sizeimage;
	}

	return 0;
}

static void list2va_driver_deinit(struct module *mod)
{
	//struct list2va_device *dev;
	//dev = (list2va_device*)module_get_driver_data(mod);
}

static int list2va_driver_handle_frame(struct module *mod)
{
	struct list2va_device *dev;
	struct shm_buff *in_frame, *out_frame;
	struct list2va_buffer *buffer;
	dev = (list2va_device*)module_get_driver_data(mod);

	in_frame = module_get_frame(mod);
	if (in_frame) {
		if (!module_if_ours_buffer(mod, in_frame)) {
			int i;
			void *in_ptr, *out_ptr;

			out_frame = module_next_alloc_buffer(mod, dev->length);
			if (out_frame) {
				void **va_list;
				va_list = (void**)shm_get_ptr(in_frame);
				out_ptr = shm_get_ptr(out_frame);
				for(i = 0; i < dev->num_planes; i++) {
					in_ptr = va_list[i];
					memcpy(out_ptr, in_ptr, dev->plane_info[i].sizeimage);
					out_ptr = out_ptr + dev->plane_info[i].sizeimage;
				}
				module_push_frame(mod, out_frame);
			}
			shm_free(in_frame);
		} else {
			buffer = (struct list2va_buffer*)in_frame->buff;
			module_push_frame(mod, buffer->shm);
			buffer->shm = NULL;
			shm_free(in_frame);
		}
	}
	usleep(1000);

	return 0;
}

module_driver list2va_driver = {
	.config = list2va_driver_config,
	.init = list2va_driver_init,
	.deinit = list2va_driver_deinit,
	.handle_frame = list2va_driver_handle_frame,
};

int list2va_module_init(struct module *mod)
{
	struct list2va_device *dev;
	dev = (list2va_device*)malloc(sizeof(list2va_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "list2va");
	module_register_driver(mod, &list2va_driver, dev);
	module_register_allocator(mod, &list2va_mem_ops, mod);
	return 0;
}

