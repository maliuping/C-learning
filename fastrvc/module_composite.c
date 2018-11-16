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
#include <cairo.h>
#include "module.h"

typedef struct composite_device {
	char	*name;
	int		rsl_width;
	int		rsl_height;
	int		disp_width;
	int		disp_height;

	// private
	double fps;
	unsigned int fps_count;
	struct timeval fps_start;
} composite_device;

/*******************************module driver**************************/

static int composite_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
			*val = MEMORY_ALLOC_SELF_OTHERS;
			break;
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_OTHERS;
			break;
		default:
			return -1;
	}
	return 0;
}

static void* composite_mem_alloc(void *private, int type, int *size)
{
	struct shm_buff *shm;
	struct module *mod;

	mod = (module*)private;

	if (type == BUFFER_OUTPUT) {
		return NULL;
	} else {
	}

	shm = module_next_alloc_buffer(mod, *size);
	if (!shm)
		return NULL;
	*size = shm->size;
	return shm;
}

static int composite_mem_free(void *private, void *buff)
{
	shm_free((shm_buff*)buff);
	return 0;
}

static void* composite_mem_get_ptr(void *private, void *buff)
{
	struct shm_buff *shm;
	shm = (struct shm_buff*)buff;

	return shm_get_ptr(shm);
}

static memory_ops composite_mem_ops = {
	.caps = composite_mem_caps,
	.alloc = composite_mem_alloc,
	.free = composite_mem_free,
	.get_ptr = composite_mem_get_ptr,
};

static void composite_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct composite_device *dev;
	dev = (composite_device*)module_get_driver_data(mod);

	if (strcmp(name, "name") == 0) {
		dev->name = strdup((char*)value);
	} else if (strcmp(name, "rsl_width") == 0) {
		dev->rsl_width = value;
	} else if (strcmp(name, "rsl_height") == 0) {
		dev->rsl_height = value;
	} else if (strcmp(name, "disp_width") == 0) {
		dev->disp_width = value;
	} else if (strcmp(name, "disp_height") == 0) {
		dev->disp_height = value;
	}
}

static int composite_driver_init(struct module *mod)
{
	// do sth.
	return 0;
}

static void composite_driver_deinit(struct module *mod)
{
	// do sth.
}

static double composite_get_fps(module *mod)
{
	struct timeval fps_end;
	unsigned int time_start, time_end;
	const int UPDATE_COUNT = 30;

	struct composite_device *dev;
	dev = (composite_device*)module_get_driver_data(mod);

	if (dev->fps_count == 0) {
		gettimeofday(&dev->fps_start, NULL);
	}
	dev->fps_count++;
	if (dev->fps_count >= UPDATE_COUNT) {
		dev->fps_count = 0;
		gettimeofday(&fps_end, NULL);
		time_start = dev->fps_start.tv_sec * 1000 + dev->fps_start.tv_usec / 1000;
		time_end = fps_end.tv_sec * 1000 + fps_end.tv_usec / 1000;
		dev->fps = UPDATE_COUNT * 1000.0 / (time_end - time_start);
	}

	return dev->fps;
}

static int composite_fps_info(module *mod, shm_buff *in_frame)
{
	void *out_buff;
	cairo_surface_t *surface;
	cairo_t *cr;
	char fps_text[512];

	struct composite_device *dev;
	dev = (composite_device*)module_get_driver_data(mod);

	out_buff = shm_get_ptr(in_frame);
	sprintf(fps_text, "fps: %f", composite_get_fps(mod));

	// create surface from out_buff
	surface = cairo_image_surface_create_for_data(out_buff, CAIRO_FORMAT_ARGB32,
						dev->rsl_width, dev->rsl_height, dev->rsl_width * 4);
	cr = cairo_create(surface);
	cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

	// draw text
	cairo_set_font_size(cr, 30);
	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_save(cr);
	cairo_rectangle(cr, 0, 0, dev->disp_width, dev->disp_height);
	cairo_clip(cr);
	cairo_translate(cr, 0, 30);
	cairo_show_text(cr, fps_text);
	cairo_restore(cr);

	// release resource
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	return 0;
}

static int composite_driver_handle_frame(struct module *mod)
{
	LOG_VERBOSE(10000, "Enter");
	struct shm_buff *in_frame;

	in_frame = module_get_frame(mod);
	if (in_frame) {
		if (module_if_ours_buffer(mod, in_frame)) {
			composite_fps_info(mod, in_frame);
			module_push_frame(mod, (struct shm_buff*)in_frame->buff);
			free(in_frame); // FIXME, cannot use shm_free here
		} else {
			composite_fps_info(mod, in_frame);
			module_push_frame(mod, in_frame);
		}
	}

	LOG_VERBOSE(10000, "Leave");
	return 0;
}

module_driver composite_driver = {
	.config = composite_driver_config,
	.init = composite_driver_init,
	.deinit = composite_driver_deinit,
	.handle_frame = composite_driver_handle_frame,
};

int composite_module_init(module *mod)
{
	struct composite_device *dev;
	dev = (composite_device*)malloc(sizeof(composite_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "composite");
	module_register_driver(mod, &composite_driver, dev);
	module_register_allocator(mod, &composite_mem_ops, mod);

	return 0;
}
