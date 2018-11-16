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
#ifndef FASTRVC_MODULE_V4L2_H
#define FASTRVC_MODULE_V4L2_H

#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "osal.h"
#include "videodev2.h"
#include "module.h"

#define VIDEO_QBUF_SUCCESS			0
#define VIDEO_QBUF_ERROR				1

/*************************v4l2 common interface*************************/

#define V4L2_DEV_ERR(dev, fmt, args...)\
	LOG_ERR("%s " fmt, dev->name, ##args)

#define V4L2_CHECK(ret, dev, fmt, args...)\
	do {\
		if (ret != 0) {\
			V4L2_DEV_ERR(dev, fmt, ##args);\
			return -1;\
		}\
	} while (0)

#define V4L2_UNTILL_SUCCESS(action) \
	do {\
		int ret;\
		do ret = action;\
		while (ret != 0);\
	} while (0)

int v4l2_ioctl (int fd, int request, void *arg);
int v4l2_open(char *device_name, int *fd);
int v4l2_close(int *fd);
int v4l2_streamon(int fd, int buf_type);
int v4l2_streamoff(int fd, int buf_type);

typedef struct v4l2_buffer_va
{
	union {
		void *va;
		void **va_list; // for multiplanar
	};
} v4l2_buffer_va;

typedef struct v4l2_buffer_info
{
	unsigned int count;
	unsigned int width;
	unsigned int height;
	unsigned int buffer_type;
	unsigned int memory_type;
	unsigned int length;
	unsigned int sizeimage;

	struct v4l2_buffer *buffer;
	struct v4l2_buffer_va *va;
	int	*buff_stat;
	MUTEX_HANDLE(mutex_stat);
	unsigned int stream_on;
	struct queue *waiting_queue;

	unsigned int pix_format;
	unsigned int pix_field;
	struct v4l2_format fmt;

	unsigned int rotation;
	struct v4l2_crop crop;
} v4l2_buffer_info;
/* flags for 'buff_stat' */
#define V4L2_BUFF_STAT_FREE 0
#define V4L2_BUFF_STAT_DEQUEUED 1
#define V4L2_BUFF_STAT_QUEUED 2
#define V4L2_BUFF_STAT_BUSY 3

int v4l2_create_buffer(int fd, v4l2_buffer_info *buf_info);
int v4l2_destroy_buffer(int fd, v4l2_buffer_info *buf_info);
int v4l2_qbuf(int fd, struct v4l2_buffer *buffer);
int v4l2_deqbuf(int fd, struct v4l2_buffer *buffer);
int v4l2_s_control(int fd, int control_type, int value);
int v4l2_s_crop(int fd, struct v4l2_crop *crop);
int v4l2_s_fmt(int fd, struct v4l2_format *fmt);
int v4l2_g_fmt(int fd, struct v4l2_format *fmt);
int v4l2_g_parm(int fd, struct v4l2_streamparm *parm);
int v4l2_fix_planeinfo(unsigned int nplanes,
	struct v4l2_plane_pix_format *plane_fmt, struct v4l2_plane *planes);
int v4l2_fix_planefmt(unsigned int format_fourcc,	unsigned int w, unsigned int h,
	struct v4l2_plane_pix_format *plane_fmt, unsigned int *nplanes, int do_align);

/*******************************v4l2 device***************************/

typedef struct v4l2_device
{
	char	*name;
	int		fd;

	struct v4l2_buffer_info	out_buff;
	struct v4l2_buffer_info	cap_buff;
	MUTEX_HANDLE(mutex_deinit);
} v4l2_device, v4l2_device_t;

#define V4L2_BUFF_OUTPUT 0
#define V4L2_BUFF_CAPTURE 1

int v4l2_device_s_fmt(v4l2_device_t *device);
int v4l2_device_init(v4l2_device_t *device);
int v4l2_device_deinit(v4l2_device_t *device);
struct v4l2_buffer* v4l2_device_deqbuf(v4l2_device_t *device, int type);
int v4l2_device_deqbuf_all(v4l2_device_t *device, int type);
int v4l2_device_qbuf(v4l2_device_t *device, struct v4l2_buffer *buffer);
int v4l2_device_qbuf_all(v4l2_device_t *device, int type);
int v4l2_device_queue_usrptr(v4l2_device_t *device, int type, void *ptr);
int v4l2_release_input_frame(module *mod);
int v4l2_push_output_frame(module *mod);
bool v4l2_device_has_free_buffer(v4l2_device_t *device, int type);
/*******************************module driver**************************/

extern memory_ops v4l2_mem_ops;
int v4l2_mem_caps(void *private, int cap, int *val);
void* v4l2_mem_alloc(void *private, int type, int *size);
int v4l2_mem_free(void *private, void *buff);
void* v4l2_mem_get_ptr(void *private, void *buff);

extern module_driver v4l2_driver;
void v4l2_driver_config(module *mod, char *name, unsigned long value);
int v4l2_driver_init(module *mod);
void v4l2_driver_deinit(module *mod);
int v4l2_driver_handle_frame(module *mod);

#endif //FASTRVC_MODULE_V4L2_H