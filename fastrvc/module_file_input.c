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
#include <sys/mman.h>
#include <malloc.h>
#include <unistd.h>
#include "module.h"

typedef struct fin_device {
	char	*name;
	FILE	*fd;
	int		buffer_size;
	char	*ptr;
	unsigned int frame_rate;
	unsigned int frame_time; // us
} fin_device;

/*******************************module driver**************************/

static int fin_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_SELF_OTHERS;
			break;
		default:
			return -1;
	}
	return 0;
}

static void* fin_mem_alloc(void *private, int type, int *size)
{
	struct fin_device *dev;
	dev = (fin_device*)private;

	*size = dev->buffer_size;
	return dev->ptr;
}

static int fin_mem_free(void *private, void *buff)
{
	return 0;
}

static memory_ops fin_mem_ops = {
	.caps = fin_mem_caps,
	.alloc = fin_mem_alloc,
	.free = fin_mem_free,
	.get_ptr = default_mem_get_ptr,
};

static void fin_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct fin_device *dev;
	dev = (fin_device*)module_get_driver_data(mod);

	if (strcmp(name, "name") == 0) {
		dev->name = strdup((char*)value);
	} else if (strcmp(name, "frame_rate") == 0) {
		dev->frame_rate = value;
	}
}

static int fin_driver_init(struct module *mod)
{
	struct fin_device *dev;
	dev = (fin_device*)module_get_driver_data(mod);

	dev->fd = fopen(dev->name, "rb");
	if (!dev->fd) {
		MOD_ERR(mod, "fopen fail: %s", dev->name);
		return -1;
	}
	fseek(dev->fd, 0, SEEK_END);
	dev->buffer_size = ftell(dev->fd);
	rewind(dev->fd);

	dev->ptr = (char*)memalign(128, dev->buffer_size);
	if (!dev->ptr) {
		MOD_ERR(mod, "malloc fail");
		fclose(dev->fd);
		return -1;
	}
	fread(dev->ptr, 1, dev->buffer_size, dev->fd);
	fclose(dev->fd);
	dev->fd = NULL;

	if (dev->frame_rate == 0)
		dev->frame_rate = 30;
	dev->frame_time = 1000000 / dev->frame_rate;

	return 0;
}

static void fin_driver_deinit(struct module *mod)
{
	struct fin_device *dev;
	dev = (fin_device*)module_get_driver_data(mod);

	dev->ptr = NULL;
	if (dev->fd)
		fclose(dev->fd);
	dev->fd = NULL;
	free(dev->ptr);
}

static int fin_driver_handle_frame(struct module *mod)
{
	struct fin_device *dev;
	struct shm_buff *out_frame;
	dev = (fin_device*)module_get_driver_data(mod);

	/*
	* push frame
	*/
	if (module_get_memory_type(mod, CAPS_MEM_OUTPUT) == MEMORY_ALLOC_OTHERS) {
		out_frame = module_next_alloc_buffer(mod, dev->buffer_size);
		if (out_frame) {
			memcpy(shm_get_ptr(out_frame), dev->ptr, dev->buffer_size);
			module_push_frame(mod, out_frame);
		}
	} else {
		out_frame = module_alloc_buffer(mod, BUFFER_OUTPUT, dev->buffer_size);
		if (out_frame) {
			module_push_frame(mod, out_frame);
		}
	}

	usleep(dev->frame_time);
	return 0;
}

module_driver fin_driver = {
	.config = fin_driver_config,
	.init = fin_driver_init,
	.deinit = fin_driver_deinit,
	.handle_frame = fin_driver_handle_frame,
};

int fin_module_init(struct module *mod)
{
	struct fin_device *dev;
	dev = (fin_device*)malloc(sizeof(fin_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "file_in");
	module_register_driver(mod, &fin_driver, dev);
	module_register_allocator(mod, &fin_mem_ops, dev);
	return 0;
}
