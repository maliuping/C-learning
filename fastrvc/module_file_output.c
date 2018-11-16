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
#include "module.h"

typedef struct fout_device {
	char	*name;
	FILE	*fd;
	unsigned int count;
	unsigned int max_count;
} fout_device;

/*******************************module driver**************************/

static memory_ops fout_mem_ops = {
	.caps = default_mem_caps,
	.alloc = default_mem_alloc,
	.free = default_mem_free,
	.get_ptr = default_mem_get_ptr,
};

static void fout_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct fout_device *dev;
	dev = (fout_device*)module_get_driver_data(mod);

	if (strcmp(name, "name") == 0) {
		dev->name = strdup((char*)value);
	} else if (strcmp(name, "max_count") == 0) {
		dev->max_count = value;
	}
}

static int fout_driver_init(struct module *mod)
{
	return 0;
}

static void fout_driver_deinit(struct module *mod)
{
	struct fout_device *dev;
	dev = (fout_device*)module_get_driver_data(mod);

	if (dev->fd)
		fclose(dev->fd);
	dev->fd = NULL;
}

static int fout_driver_handle_frame(struct module *mod)
{
	char path[1024];
	struct fout_device *dev;
	struct shm_buff *in_frame;
	dev = (fout_device*)module_get_driver_data(mod);

	/*
	* queue input frame
	*/
	in_frame = module_get_frame(mod);
	if (in_frame) {
		LOG_ONCE("display first frame");
		if (!dev->fd) {
			sprintf(path, "%s_%d", dev->name, dev->count++);
			dev->fd = fopen(path, "wb");
		}

		if (!dev->fd) {
			MOD_ERR(mod, "fopen fail");
		} else {
			fwrite(shm_get_ptr(in_frame), 1, in_frame->size, dev->fd);
			fflush(dev->fd);
			if (dev->max_count != 0) {
				fclose(dev->fd);
				dev->fd = NULL;
			}
		}
		module_push_frame(mod, in_frame);

		if (dev->count >= dev->max_count)
			dev->count = 0;
		LOG_FPS("%s", mod->name);
	}

	return 0;
}

module_driver fout_driver = {
	.config = fout_driver_config,
	.init = fout_driver_init,
	.deinit = fout_driver_deinit,
	.handle_frame = fout_driver_handle_frame,
};

int fout_module_init(struct module *mod)
{
	struct fout_device *dev;
	dev = (fout_device*)malloc(sizeof(fout_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "file_out");
	module_register_driver(mod, &fout_driver, dev);
	module_register_allocator(mod, &fout_mem_ops, dev);
	return 0;
}
