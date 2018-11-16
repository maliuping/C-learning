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
#include "module_v4l2.h"

#define V4L2_CID_USER_MTK_MDP_BASE           (V4L2_CID_USER_BASE + 0x1090)
#define V4L2_CID_MDP_SHARPNESS_ENABLE        (V4L2_CID_USER_MTK_MDP_BASE + 0)
#define V4L2_CID_MDP_SHARPNESS_LEVEL         (V4L2_CID_USER_MTK_MDP_BASE + 1)
#define V4L2_CID_MDP_DYNAMIC_CONTRAST_ENABLE (V4L2_CID_USER_MTK_MDP_BASE + 2)

typedef struct v4l2_mdp_device {
	// common
	v4l2_device v4l2_base;
	// private
	unsigned int shp_enable; // PQ sharpness_enable
	unsigned int shp_level; // PQ sharpness_level
	unsigned int dc_enable; // PQ dynamic_contrast_enable
} v4l2_mdp_device;

#define MAX_SHARPNESS_LEVEL 11

static void v4l2_mdp_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct v4l2_mdp_device *dev;
	dev = (v4l2_mdp_device*)module_get_driver_data(mod);

	if (strcmp(name, "shp_enable") == 0) {
		dev->shp_enable = value;;
	} else if (strcmp(name, "shp_level") == 0) {
		dev->shp_level = value;;
	} else if (strcmp(name, "dc_enable") == 0) {
		dev->dc_enable = value;;
	} else {
		v4l2_driver_config(mod, name, value);
	}
}

static int v4l2_mdp_driver_init(struct module *mod)
{
	struct v4l2_device *dev;
	struct v4l2_mdp_device *mdp_dev;
	mdp_dev = (v4l2_mdp_device*)module_get_driver_data(mod);
	dev = (v4l2_device*)mdp_dev;

	// do base init
	if (v4l2_driver_init(mod) != 0)
		return -1;

	// mdp need config more
	if (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == dev->out_buff.buffer_type)
		dev->out_buff.crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	else
		dev->out_buff.crop.type = dev->out_buff.buffer_type;

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == dev->cap_buff.buffer_type)
		dev->cap_buff.crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	else
		dev->cap_buff.crop.type = dev->cap_buff.buffer_type;

	v4l2_s_crop(dev->fd, &dev->out_buff.crop);
	v4l2_s_crop(dev->fd, &dev->cap_buff.crop);

	v4l2_s_control(dev->fd, V4L2_CID_ROTATE, dev->out_buff.rotation);
	v4l2_s_control(dev->fd, V4L2_CID_HFLIP, 0);
	v4l2_s_control(dev->fd, V4L2_CID_VFLIP, 0);
	v4l2_s_control(dev->fd, V4L2_CID_ALPHA_COMPONENT, 255);

	if (mdp_dev->shp_enable) {
		v4l2_s_control(dev->fd, V4L2_CID_MDP_SHARPNESS_ENABLE, 1);

		if (mdp_dev->shp_level > MAX_SHARPNESS_LEVEL)
			mdp_dev->shp_level = MAX_SHARPNESS_LEVEL;
		v4l2_s_control(dev->fd, V4L2_CID_MDP_SHARPNESS_LEVEL, mdp_dev->shp_level);

		if (mdp_dev->dc_enable)
			v4l2_s_control(dev->fd, V4L2_CID_MDP_DYNAMIC_CONTRAST_ENABLE, 1);
	}
	return 0;
}

static module_driver v4l2_mdp_driver = {
	.config = v4l2_mdp_driver_config,
	.init = v4l2_mdp_driver_init,
	.deinit = v4l2_driver_deinit,
	.handle_frame = v4l2_driver_handle_frame,
};

int v4l2_mdp_module_init(module *mod)
{
	struct v4l2_mdp_device *dev;
	dev = (v4l2_mdp_device*)malloc(sizeof(v4l2_mdp_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "v4l2_mdp");
	module_register_driver(mod, &v4l2_mdp_driver, dev);
	module_register_allocator(mod, &v4l2_mem_ops, dev);
	return 0;
}
