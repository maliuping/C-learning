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
#include <sys/mount.h>
#include <time.h>

#define INIT_LOG(fmt, args...)\
	do {\
		struct timespec ts;\
		clock_gettime(CLOCK_BOOTTIME, &ts);\
		fprintf(stderr, "[%5lu.%6lu]" "[MTK_RVC]" fmt "\n",\
				ts.tv_sec, ts.tv_nsec / 1000, ##args);\
		fflush(stderr);\
	} while (0)

#ifdef FASTRVC_INIT_DEV
extern void device_mknod_init();
#endif

int main(int argc, char *argv[])
{
	int ret;
	pid_t pid;

	INIT_LOG("initrvc start");

	ret = mount("sysfs", "/sys", "sysfs", 0, 0);
	if (ret)
		INIT_LOG("initrvc mount failed");

	pid = fork();
	if (pid == 0) {
		// child
		char *argv[] = {FASTRVC_PROGRAM, FASTRVC_CONFIG, "--first_stage", "--logo", NULL};
		INIT_LOG("initrvc start fastrvc");
		if (access("/sbin/recovery", F_OK) == 0) {
			INIT_LOG("initrvc detected recovery mode, stoped");
			return 0;
		}

#ifdef FASTRVC_INIT_DEV
		/*
		* add this for quick create /dev/xxx which fastrvc needed
		* before Android ueventd ready
		*/
		device_mknod_init();
#endif

		INIT_LOG("initrvc start fastrvc");
		execve(FASTRVC_PROGRAM, argv, NULL);
	} else if (pid > 0) {
		// parent
		char *argv[] = {SYSTEM_INIT_PROGRAM, NULL};
		execve(SYSTEM_INIT_PROGRAM, argv, NULL);
	} else {
		INIT_LOG("initrvc fork error\n");
	}
	return 0;
}
