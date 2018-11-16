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
#include <stdio.h>
#include <string.h>
#include "module.h"

typedef struct ppm_logo_device {
	char	*name;
	FILE	*fd;
	int		width;
	int		height;
	int		length;
	void	*ptr;
} ppm_logo_device;

/*******************************module driver**************************/

static int ppm_logo_mem_caps(void *private, int cap, int *val)
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

static void* ppm_logo_mem_alloc(void *private, int type, int *size)
{
	struct ppm_logo_device *dev;
	dev = (ppm_logo_device*)private;

	*size = dev->length;
	return dev->ptr;
}

static int ppm_logo_mem_free(void *private, void *buff)
{
	return 0;
}

static memory_ops ppm_logo_mem_ops = {
	.caps = ppm_logo_mem_caps,
	.alloc = ppm_logo_mem_alloc,
	.free = ppm_logo_mem_free,
	.get_ptr = default_mem_get_ptr,
};

static void ppm_logo_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct ppm_logo_device *dev;
	dev = (ppm_logo_device*)module_get_driver_data(mod);

	if (strcmp(name, "name") == 0) {
		dev->name = strdup((char*)value);
	}
}

static int ppm_logo_driver_init(struct module *mod)
{
	int size;
	int pix_max;
	char header0[32];
	char header1[32];
	char header2[32];
	struct ppm_logo_device *dev;
	dev = (ppm_logo_device*)module_get_driver_data(mod);

	dev->fd = fopen(dev->name, "rb");
	if (!dev->fd) {
		MOD_ERR(mod, "open fail: %s", dev->name);
		return -1;
	}

	/* ppm format */
	fgets(header0, sizeof(header0), dev->fd);
	fgets(header1, sizeof(header1), dev->fd);
	fgets(header2, sizeof(header2), dev->fd);
	sscanf(header1, "%d %d\n", &dev->width, &dev->height);
	sscanf(header2, "%d\n", &pix_max);

	if ((dev->width == 0) || (dev->height == 0)) {
		MOD_ERR(mod, "read logo fail: width=%d height=%d", dev->width, dev->height);
		goto err;
	}

	switch (pix_max) {
		case 255:
			size = 3; break;
		default:
			MOD_ERR(mod, "can't support pix_max %d", pix_max);
			goto err;
	}

	dev->length = dev->width * dev->height * size;
	dev->ptr = malloc(dev->length);
	if (dev->ptr == NULL) {
		MOD_ERR(mod, "allocate memory fail");
		goto err;
	}

	fread(dev->ptr, 1, dev->length, dev->fd);
	fclose(dev->fd);
	dev->fd = NULL;
	return 0;
err:
	fclose(dev->fd);
	dev->fd = NULL;
	return -1;
}

static void ppm_logo_driver_deinit(struct module *mod)
{
	struct ppm_logo_device *dev;
	dev = (ppm_logo_device*)module_get_driver_data(mod);

	if (dev->ptr)
		free(dev->ptr);
	if (dev->fd)
		fclose(dev->fd);
	dev->ptr = NULL;
	dev->fd = NULL;
}

static int ppm_logo_driver_handle_frame(struct module *mod)
{
	struct ppm_logo_device *dev;
	struct shm_buff *out_frame;
	dev = (ppm_logo_device*)module_get_driver_data(mod);

	/*
	* queue input frame
	*/
	out_frame = module_alloc_buffer(mod, CAPS_MEM_OUTPUT, dev->length);
	if (out_frame) {
		module_push_frame(mod, out_frame);
	}

	usleep(100000);
	return 0;
}

module_driver ppm_logo_driver = {
	.config = ppm_logo_driver_config,
	.init = ppm_logo_driver_init,
	.deinit = ppm_logo_driver_deinit,
	.handle_frame = ppm_logo_driver_handle_frame,
};

int ppm_logo_module_init(struct module *mod)
{
	struct ppm_logo_device *dev;
	dev = (ppm_logo_device*)malloc(sizeof(ppm_logo_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "ppm_logo");
	module_register_driver(mod, &ppm_logo_driver, dev);
	module_register_allocator(mod, &ppm_logo_mem_ops, dev);
	return 0;
}
