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
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#define DEV_INIT_LOG(fmt, args...)\
	do {\
		struct timespec ts;\
		clock_gettime(CLOCK_BOOTTIME, &ts);\
		fprintf(stderr, "[%5lu.%6lu]" "[MTK_RVC]" fmt "\n",\
				ts.tv_sec, ts.tv_nsec / 1000, ##args);\
		fflush(stderr);\
	} while (0)

static void wait_dev_mount()
{
	FILE *fd;
	char buff[1024];
	char *proc_mounts = "/proc/mounts";

	DEV_INIT_LOG("wait /dev mount");

	fd = NULL;
	while (!fd) {
		fd = fopen(proc_mounts, "r");
	}
	while (1) {
		if (fgets(buff, 1024, fd)) {
			if (strstr(buff, " /dev "))
				break;
		} else {
			fflush(fd);
			DEV_INIT_LOG("waiting /dev ...");
		}
	}

	fclose(fd);
	DEV_INIT_LOG("/dev mount ok");
}

void device_mknod_init()
{
	int i;
	FILE *file;
	FILE *file_dev;
	char dev_path[256];
	char dev_num_path[256];
	int fd;
	int major, minor;
	int ret_mknod = 0;

	DEV_INIT_LOG("device_mknod_init start");

	/*
	* can't mount /dev directly because Android used tmpfs filesystem for /dev
	* and ueventd will mount again, this action will cause all nodes created been removed
	*/
	wait_dev_mount();

	/*
	* make device nodes for /sys/class/video4linux/*
	*/
	for (i = 0; i < 100; i++) {
		sprintf(dev_path, "/dev/video%d", i);
		sprintf(dev_num_path, "/sys/class/video4linux/video%d/dev", i);

		file_dev = fopen(dev_num_path, "r");
		if (!file_dev)
			continue;
		fscanf(file_dev, "%d:%d", &major, &minor);
		fclose(file_dev);

		ret_mknod = mknod(dev_path, S_IFCHR | 0666, makedev(major, minor));
		if (ret_mknod != 0) {
			DEV_INIT_LOG("error: mknod %s", dev_path);
			continue;
		}
		chmod(dev_path, 0666);
	}

	/*
	* make device node for drm
	*/
	mkdir("/dev/dri/", 0755);
	for (i = 0; i < 10; i++) {
		sprintf(dev_num_path, "/sys/class/drm/card%d/dev", i);
		sprintf(dev_path, "/dev/dri/card%d", i);
		file_dev = fopen(dev_num_path, "r");
		if (file_dev != NULL) {
			fscanf(file_dev, "%d:%d", &major, &minor);
			fclose(file_dev);

			ret_mknod = mknod(dev_path, S_IFCHR | 0666, makedev(major, minor));
			if (ret_mknod != 0) {
				DEV_INIT_LOG("error: mknod %s", dev_path);
				continue;
			}
			chmod(dev_path, 0666);
			break;
		}
	}


	/*
	* make device node for mmcblk
	*/
    mkdir("/dev/block/", 0755);
	for (i = 0; i < 10; i++) {
		sprintf(dev_num_path, "/sys/class/block/mmcblk%d/dev", i);
		sprintf(dev_path, "/dev/block/mmcblk%d", i);
		file_dev = fopen(dev_num_path, "r");
		if (file_dev != NULL) {
			fscanf(file_dev, "%d:%d", &major, &minor);
			fclose(file_dev);

			ret_mknod = mknod(dev_path, S_IFBLK | 0600, makedev(major, minor));
			if (ret_mknod != 0) {
				DEV_INIT_LOG("error: mknod %s", dev_path);
				continue;
			}
			chmod(dev_path, 0666);
			break;
		}
	}

	DEV_INIT_LOG("device_mknod_init exit");
}
