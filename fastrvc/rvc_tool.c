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
#include <sys/prctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include "module.h"
#include "device_config.h"
#include "drm_display.h"
#include "util.h"
#include "xml.h"

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

typedef struct v4l2_nr_device {
	// common
	v4l2_device v4l2_base;
	// private
	unsigned int bnr_level;
	unsigned int mnr_level;
	unsigned int fnr_level;
} v4l2_nr_device;

static module *mod_mdp = NULL;
static module *mod_nr = NULL;
static int stop_flag = 0;

static void process_cmd(char *name, int value)
{
	unsigned int nr_level;
	v4l2_nr_device *nr_dev = NULL;
	v4l2_mdp_device *mdp_dev = NULL;

	LOG_INFO("process_cmd: %s %d", name, value);

	if (mod_nr) {
		nr_dev = (v4l2_nr_device*)module_get_driver_data(mod_nr);
		if (strstr(name, "bnr_level")) {
			nr_dev->bnr_level = value;
			nr_level = (value << 16) | (nr_dev->mnr_level << 8) | nr_dev->fnr_level;
			v4l2_ioctl(nr_dev->v4l2_base.fd, VIDIOC_S_INPUT, &nr_level);
			return;
		} else if (strstr(name, "mnr_level")) {
			nr_dev->mnr_level = value;
			nr_level = (nr_dev->bnr_level << 16) | (value << 8) | nr_dev->fnr_level;
			v4l2_ioctl(nr_dev->v4l2_base.fd, VIDIOC_S_INPUT, &nr_level);
			return;
		} else if (strstr(name, "fnr_level")) {
			nr_dev->fnr_level = value;
			nr_level = (nr_dev->bnr_level << 16) | (nr_dev->mnr_level << 8) | value;
			v4l2_ioctl(nr_dev->v4l2_base.fd, VIDIOC_S_INPUT, &nr_level);
			return;
		}
	}

	if (mod_mdp) {
		mdp_dev = (v4l2_mdp_device*)module_get_driver_data(mod_mdp);
		if (strstr(name, "shp_enable")) {
			mdp_dev->shp_enable = value;
			v4l2_s_control(mdp_dev->v4l2_base.fd, V4L2_CID_MDP_SHARPNESS_ENABLE, value);
			return;
		} else if (strstr(name, "shp_level")) {
			mdp_dev->shp_level = value;
			if (mdp_dev->shp_enable)
				v4l2_s_control(mdp_dev->v4l2_base.fd, V4L2_CID_MDP_SHARPNESS_LEVEL, value);
			return;
		} else if (strstr(name, "dc_enable")) {
			mdp_dev->dc_enable = value;
			if (mdp_dev->shp_enable)
				v4l2_s_control(mdp_dev->v4l2_base.fd, V4L2_CID_MDP_DYNAMIC_CONTRAST_ENABLE, value);
			return;
		}
	}

	LOG_ERR("error command");
}

static void* camera_tool_thread(void *data)
{
	prctl(PR_SET_NAME, "camera_tool");
	LOG_INFO("camera_tool_thread start");

	link_path *link;
	FILE *fp = NULL;
	char name[256];
	int value;
	char *file_path = "/tmp/rvc_tool_cmd";

	module *mod;
	link = (link_path*)data;

	mod = link->head;
	while (mod) {
		if (strstr(mod->name, "v4l2_mdp")) {
			mod_mdp = mod;
		} else if (strstr(mod->name, "v4l2_nr")) {
			mod_nr = mod;
		}
		mod = mod->next;
	}

	name[0] = 0;
	while (1) {
		fp = fopen(file_path, "r");
		if (fp) {
			fscanf(fp, "%s %d", name, &value);
			if (name[0] != 0) {
				process_cmd(name, value);
				name[0] = 0;
			}
			fclose(fp);
			remove(file_path);
		}

		if (stop_flag)
			break;

		usleep(50000);
	}

	return NULL;
}

static void sig_handler(int sig)
{
	static int again = 0;
	if (again)
		exit(0);
	again = 1;

	switch (sig) {
		default:
		case SIGINT:
			stop_flag = 1;
			break;
	}
}

int main(int argc, char *argv[])
{
	void *xml_handle;
	link_path *link;
	THREAD_HANDLE(tool_thread);

	/* signal handler */
	struct sigaction act;
	act.sa_handler = sig_handler;
	sigaction(SIGINT, &act, NULL);

	xml_handle = xml_open(argv[1]);
	if (!xml_handle) {
		LOG_ERR("open setting file fail");
		return -1;
	}

	log_level_init(xml_handle);
	device_path_init();

	link = xml_create_camera(xml_handle);
	if (link == NULL) {
		LOG_ERR("create link fail");
		return -1;
	}

	if (0 != link_init_modules(link)) {
		LOG_ERR("link_init_modules fail");
		return -1;
	}

	if (0 != link_active_modules(link)) {
		LOG_ERR("link_active_modules fail");
		link_stop_modules(link);
		return -1;
	}

	THREAD_CREATE(tool_thread, camera_tool_thread, link);
	THREAD_WAIT(tool_thread);

	link_stop_modules(link);
}
