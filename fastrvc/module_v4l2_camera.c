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

static int v4l2_camera_mem_caps(void *private, int cap, int *val)
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

static memory_ops v4l2_camera_mem_ops = {
	.caps = v4l2_camera_mem_caps,
	.alloc = v4l2_mem_alloc,
	.free = v4l2_mem_free,
	.get_ptr = v4l2_mem_get_ptr,
};

static int v4l2_camera_driver_handle_frame(struct module *mod)
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

static module_driver v4l2_camera_driver = {
	.config = v4l2_driver_config,
	.init = v4l2_driver_init,
	.deinit = v4l2_driver_deinit,
	.handle_frame = v4l2_camera_driver_handle_frame,
};

int v4l2_camera_module_init(module *mod)
{
	struct v4l2_device *dev;
	dev = (v4l2_device*)malloc(sizeof(v4l2_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "v4l2_camera");
	module_register_driver(mod, &v4l2_camera_driver, dev);
	module_register_allocator(mod, &v4l2_camera_mem_ops, dev);
	return 0;
}
