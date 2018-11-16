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
#include <malloc.h>
#include <unistd.h>
#include "module.h"

typedef struct h264_logo_device {
	char	*name;
	FILE	*fd;
	int		width;
	int		height;
	int		length;
	char	*start_pos;
	char	*sps_pos;
	char	*current_pos;
	char	*next_pos;
	char	*end_pos;
	unsigned int frame_rate;
	unsigned int frame_time; // us
} h264_logo_device;

/*******************************module driver**************************/

static int h264_logo_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_SELF_OTHERS;
			break;
		default:
			return -1;
	}
	return 0;
}

static char* skip_start_code(char *stream)
{
	if ((stream[0] == 0x00) && (stream[1] == 0x00)) {
		if ((stream[2] == 0x00) && (stream[3] == 0x01))
			return &stream[4];
		else if (stream[2] == 0x01)
			return &stream[3];
		else
			return NULL;
	}
	return NULL;
}

static char* find_start_code(char *start, char *end)
{
	while (start < end) {
		if (skip_start_code(start))
			return start;
		start++;
	}
	return NULL;
}

static char* find_sps_code(char *start, char *end)
{
	while (start < end){
		if ((start[0] == 0x00) && (start[1] == 0x00) &&
			(start[2] == 0x01) && ((start[3] & 0x0f) == 0x07))
			return start;
		start ++;
	}
	return NULL;
}

static void* h264_logo_mem_alloc(void *private, int type, int *size)
{
	struct h264_logo_device *dev;
	dev = (h264_logo_device*)private;

	dev->current_pos = dev->next_pos;
	dev->next_pos = skip_start_code(dev->current_pos);
	dev->next_pos = find_start_code(dev->next_pos, dev->end_pos); // find next frame
	if (dev->next_pos == NULL) {
		// restart
		dev->next_pos = dev->sps_pos;
		*size = dev->end_pos - dev->current_pos;
	} else {
		*size = dev->next_pos - dev->current_pos;
	}

	return dev->current_pos;
}

static int h264_logo_mem_free(void *private, void *buff)
{
	return 0;
}

static memory_ops h264_logo_mem_ops = {
	.caps = h264_logo_mem_caps,
	.alloc = h264_logo_mem_alloc,
	.free = h264_logo_mem_free,
	.get_ptr = default_mem_get_ptr,
};

static void h264_logo_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct h264_logo_device *dev;
	dev = (h264_logo_device*)module_get_driver_data(mod);

	if (strcmp(name, "name") == 0) {
		dev->name = strdup((char*)value);
	} else if (strcmp(name, "frame_rate") == 0) {
		dev->frame_rate = value;
	}
}

static int h264_logo_driver_init(struct module *mod)
{
	struct h264_logo_device *dev;
	dev = (h264_logo_device*)module_get_driver_data(mod);

	dev->fd = fopen(dev->name, "rb");
	if (!dev->fd) {
		MOD_ERR(mod, "open fail: %s", dev->name);
		return -1;
	}
	fseek(dev->fd, 0, SEEK_END);
	dev->length = ftell(dev->fd);
	rewind(dev->fd);
	dev->start_pos = (char*)memalign(64, dev->length);
	if (dev->start_pos == NULL) {
		MOD_ERR(mod, "allocate memory fail");
		goto err;
	}

	fread(dev->start_pos, 1, dev->length, dev->fd);
	fclose(dev->fd);
	dev->fd = NULL;

	dev->end_pos = dev->start_pos + dev->length;
	dev->sps_pos = find_sps_code(dev->start_pos, dev->end_pos);
	if (dev->sps_pos == NULL) {
		MOD_ERR(mod, "H264 bitstream has no sps code");
		goto err1;
	}

	dev->current_pos = dev->next_pos = dev->sps_pos;

	if (dev->frame_rate == 0)
		dev->frame_rate = 30;
	dev->frame_time = 1000000 / dev->frame_rate;

	return 0;

err:
	fclose(dev->fd);
	dev->fd = NULL;
	return -1;
err1:
	free(dev->start_pos);
	return -1;
}

static void h264_logo_driver_deinit(struct module *mod)
{
	struct h264_logo_device *dev;
	dev = (h264_logo_device*)module_get_driver_data(mod);

	if (dev->start_pos)
		free(dev->start_pos);
	if (dev->fd)
		fclose(dev->fd);
	dev->start_pos = NULL;
	dev->fd = NULL;
}

static int h264_logo_driver_handle_frame(struct module *mod)
{
	struct h264_logo_device *dev;
	struct shm_buff *in_frame, *out_frame;
	dev = (h264_logo_device*)module_get_driver_data(mod);

	/*
	* queue input frame
	*/
	if (module_get_memory_type(mod, CAPS_MEM_OUTPUT) == MEMORY_ALLOC_SELF) {
		out_frame = module_alloc_buffer(mod, CAPS_MEM_OUTPUT, 0);
		if (out_frame) {
			module_push_frame(mod, out_frame);
		}
	} else {
		out_frame = module_next_alloc_buffer(mod, 0);
		if (out_frame) {
			in_frame = module_alloc_buffer(mod, CAPS_MEM_OUTPUT, 0);
			out_frame->size = in_frame->size;
			memcpy(shm_get_ptr(out_frame), shm_get_ptr(in_frame), in_frame->size);
			module_push_frame(mod, out_frame);
			shm_free(in_frame);
		}
	}

	usleep(dev->frame_time);
	return 0;
}

module_driver h264_logo_driver = {
	.config = h264_logo_driver_config,
	.init = h264_logo_driver_init,
	.deinit = h264_logo_driver_deinit,
	.handle_frame = h264_logo_driver_handle_frame,
};

int h264_logo_module_init(struct module *mod)
{
	struct h264_logo_device *dev;
	dev = (h264_logo_device*)malloc(sizeof(h264_logo_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "h264_logo");
	module_register_driver(mod, &h264_logo_driver, dev);
	module_register_allocator(mod, &h264_logo_mem_ops, dev);
	return 0;
}
