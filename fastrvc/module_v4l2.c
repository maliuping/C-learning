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

//2 Global variable
static int qbuf_error_flag = VIDEO_QBUF_SUCCESS;
/*************************v4l2 common interface*************************/

int v4l2_ioctl (int fd, int request, void *arg) {
	int r;
	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
	return r;
}

int v4l2_open(char *device_name, int *fd)
{
	struct stat st;
	if (-1 == stat(device_name, &st)) {
		return -1;
	}
	if (!S_ISCHR(st.st_mode)) {
		return -1;
	}
	/*
	* pls always use non-block mode, we need to handle other events in main thread
	*/
	*fd = open(device_name, O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
	if (-1 == *fd) {
		return -1;
	}
	return 0;
}

int v4l2_close(int *fd)
{
	if (-1 == close(*fd)) {
		*fd = -1;
		return -1;
	}
	*fd = -1;
	return 0;
}

int v4l2_streamon(int fd, int buf_type)
{
	return v4l2_ioctl(fd, VIDIOC_STREAMON, &buf_type);
}

int v4l2_streamoff(int fd, int buf_type)
{
	return v4l2_ioctl(fd, VIDIOC_STREAMOFF, &buf_type);
}

int v4l2_create_buffer(int fd, v4l2_buffer_info *buf_info)
{
	int i, j;
	int *buff_stat;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer *buffer;
	struct v4l2_buffer_va *va;

	if (buf_info->count <= 0)
		return 0;

	req.count = buf_info->count;
	req.type = buf_info->buffer_type;
	req.memory = buf_info->memory_type;

	if (0 != v4l2_ioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
		} else {
		}
		return -1;
	}
	buf_info->count = req.count;
	buffer = (struct v4l2_buffer*)malloc(sizeof(struct v4l2_buffer) * req.count);
	memset(buffer, 0, sizeof(struct v4l2_buffer) * req.count);
	va = (v4l2_buffer_va*)malloc(sizeof(v4l2_buffer_va) * req.count);
	memset(va, 0, sizeof(v4l2_buffer_va) * req.count);
	buff_stat = (int*)malloc(sizeof(int) * req.count);
	memset(buff_stat, 0, sizeof(int) * req.count);

	if (V4L2_TYPE_IS_MULTIPLANAR(req.type)) {
		for (i = 0; i < req.count; i++) {
			buffer[i].m.planes =
				(struct v4l2_plane*)malloc(sizeof(struct v4l2_plane) * buf_info->length);
			memset(buffer[i].m.planes, 0, sizeof(struct v4l2_plane) * buf_info->length);

			v4l2_fix_planeinfo(buf_info->length, buf_info->fmt.fmt.pix_mp.plane_fmt, buffer[i].m.planes);
		}
	}

	for(i = 0; i < req.count; i++) {
		buffer[i].index = i;
		buffer[i].type = buf_info->buffer_type;
		buffer[i].memory = buf_info->memory_type;
		buffer[i].length =  buf_info->length;

		if (0 != v4l2_ioctl(fd, VIDIOC_QUERYBUF, &buffer[i])) {
			return -1; //FIXME
		}

		// fix again for 'bytesused'
		if (V4L2_TYPE_IS_MULTIPLANAR(req.type))
			v4l2_fix_planeinfo(buf_info->length, buf_info->fmt.fmt.pix_mp.plane_fmt, buffer[i].m.planes);
		else
			buffer[i].bytesused = buffer[i].length;
	}

	if (req.memory == V4L2_MEMORY_MMAP) {
		if (V4L2_TYPE_IS_MULTIPLANAR(req.type)) {
			for (i = 0; i < req.count; i++) {
				va[i].va_list = (void**)malloc(sizeof(void*) * (buffer[i].length + 1));
				for (j = 0; j < buffer[i].length; j++) {
					va[i].va_list[j] =
						mmap(NULL, buffer[i].m.planes[j].length,
								PROT_READ | PROT_WRITE, MAP_SHARED,
									fd, buffer[i].m.planes[j].m.mem_offset);
					if (MAP_FAILED == va[i].va_list[j]) {
						LOG_ERR("mmap failed");
						return -1; //FIXME
					}
				}
				va[i].va_list[j] = NULL;
			}
		} else {
			for (i = 0; i < req.count; i++) {
				va[i].va =
					mmap(NULL, buffer[i].length,
							PROT_READ | PROT_WRITE, MAP_SHARED,
								fd, buffer[i].m.offset);
				if (MAP_FAILED == va[i].va) {
					return -1; //FIXME
				}
			}
		}
	}

	buf_info->buffer = buffer;
	buf_info->va = va;
	buf_info->buff_stat = buff_stat;
	MUTEX_INIT(buf_info->mutex_stat);

	return 0;
}

int v4l2_destroy_buffer(int fd, v4l2_buffer_info *buf_info)
{
	int i, j;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer *buffer;
	struct v4l2_buffer_va *va;

	if (buf_info->count <= 0)
		return 0;
	if (buf_info->buffer == NULL)
		return 0;

	va = buf_info->va;
	buffer = buf_info->buffer;
	req.count = buf_info->count;
	req.type = buf_info->buffer_type;
	req.memory = buf_info->memory_type;

	if (buf_info->stream_on == 1) {
		V4L2_UNTILL_SUCCESS(
			v4l2_streamoff(fd, buf_info->buffer_type)
			);
		buf_info->stream_on = 0;
	}

	if (req.memory == V4L2_MEMORY_MMAP) {
		if (V4L2_TYPE_IS_MULTIPLANAR(req.type)) {
			for (i = 0; i < req.count; i++) {
				for (j = 0; j < buffer[i].length; j++) {
					munmap(va[i].va_list[j], buffer[i].m.planes[j].length);
				}
				free(va[i].va_list);
			}
		} else {
			for (i = 0; i < req.count; i++) {
				munmap(va[i].va, buffer[i].length);
			}
		}
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(req.type)) {
		for (i = 0; i < req.count; i++)
			free(buffer[i].m.planes);
	}

	free(buf_info->buffer);
	free(buf_info->va);
	free(buf_info->buff_stat);
	buf_info->buffer = NULL;
	buf_info->va = NULL;
	buf_info->buff_stat = NULL;

	req.count = 0;
	v4l2_ioctl(fd, VIDIOC_REQBUFS, &req);

	return 0;
}

int v4l2_deqbuf(int fd, struct v4l2_buffer *buffer)
{
	return v4l2_ioctl(fd, VIDIOC_DQBUF, buffer);
}

int v4l2_qbuf(int fd, struct v4l2_buffer *buffer)
{
	return v4l2_ioctl(fd, VIDIOC_QBUF, buffer);
}

int v4l2_s_control(int fd, int control_type, int value)
{
	struct v4l2_control control;

	control.id = control_type;
	control.value = value;

	return v4l2_ioctl(fd, VIDIOC_S_CTRL, &control);
}

int v4l2_s_crop(int fd, struct v4l2_crop *crop)
{
	struct v4l2_selection sel = {0};

	sel.type = crop->type;
	sel.r = crop->c;

	if (sel.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		sel.target = V4L2_SEL_TGT_CROP;
	} else if (sel.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		sel.target = V4L2_SEL_TGT_COMPOSE;
	} else {
		LOG_ERR("format not support");
		return -1;
	}

	return v4l2_ioctl(fd, VIDIOC_S_SELECTION, &sel);
}

int v4l2_s_fmt(int fd, struct v4l2_format *fmt)
{
	return v4l2_ioctl(fd, VIDIOC_S_FMT, fmt);
}

int v4l2_g_fmt(int fd, struct v4l2_format *fmt)
{
	return v4l2_ioctl(fd, VIDIOC_G_FMT, fmt);
}

int v4l2_g_parm(int fd, struct v4l2_streamparm *parm)
{
	return v4l2_ioctl(fd, VIDIOC_G_PARM, parm);
}

int v4l2_fix_planeinfo(unsigned int nplanes,
	struct v4l2_plane_pix_format *plane_fmt, struct v4l2_plane *planes)
{
	int i;
	for (i = 0; i < nplanes; i++) {
		planes[i].length = plane_fmt[i].sizeimage;
		planes[i].bytesused = plane_fmt[i].sizeimage;
	}
	return 0;
}

int v4l2_fix_planefmt(unsigned int format_fourcc,	unsigned int w, unsigned int h,
	struct v4l2_plane_pix_format *plane_fmt, unsigned int *nplanes, int do_align)
{
	int align_w, align_h, uv_align_w, uv_align_h;
	switch (format_fourcc) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV21M:
		/* YUV 420 scanline, 2 planes. Y & C planes are not contiguous in memory. */
		// src/dst fmt
		*nplanes = 2;
		if (0 && do_align) {
			align_w = (w + 15) & (~15);
			align_h = (h + 15) & (~15);
		} else {
			align_w = w;
			align_h = h;
		}
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* CbCr plane */
		plane_fmt[1].bytesperline = align_w / 2;
		plane_fmt[1].sizeimage = align_h * align_w / 2;
		break;
	case V4L2_PIX_FMT_MT21:
	{
		unsigned int y_plane_size, c_plane_size;
		/* YUV 420 blk UFO, Y & C planes are not contiguous in memory. */
		*nplanes = 2;
		if (do_align) {
			align_w = (w + 15) & (~15);
			align_h = (h + 31) & (~31);
		} else {
			align_w = w;
			align_h = h;
		}

		y_plane_size = align_w * align_h;
		c_plane_size = ((align_h / 2 + 31) & (~31)) * align_w;

		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = y_plane_size;
		/* CbCr plane */
		plane_fmt[1].bytesperline = align_w / 2;
		plane_fmt[1].sizeimage = c_plane_size;
		break;
	}
	case V4L2_PIX_FMT_YUV422P:
		/* YUV 422 planer*/
		// src fmt
		*nplanes = 3;
		if (0 && do_align) {
			align_w = (w + 15) & (~15);
			align_h = (h + 15) & (~15);
		} else {
			align_w = w;
			align_h = h;
		}
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* U/V plane */
		uv_align_w = (align_w/2 + 15) & (~15);
		uv_align_h = (align_h + 7) & (~7);
		plane_fmt[1].bytesperline = uv_align_w;
		plane_fmt[1].sizeimage = uv_align_h * uv_align_w;
		/* V/U plane */
		plane_fmt[2].bytesperline = plane_fmt[1].bytesperline;
		plane_fmt[2].sizeimage = plane_fmt[1].sizeimage;
		break;
	case V4L2_PIX_FMT_YUV420M:
		/* YUV 420 planer*/
		// dst fmt
		*nplanes = 3;
		align_w = w;
		align_h = h;
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* U/V plane */
		plane_fmt[1].bytesperline = align_w/2;
		plane_fmt[1].sizeimage = align_h/2 * align_w/2;
		/* V/U plane */
		plane_fmt[2].bytesperline = align_w/2;
		plane_fmt[2].sizeimage = align_h/2 * align_w/2;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV16M:
		/* YUV 422 scanline, 2 planes. */
		*nplanes = 2;
		align_w = (w + 15) & (~15);
		align_h = (h + 31) & (~31);
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* CbCr plane */
		plane_fmt[1].bytesperline = align_w;
		plane_fmt[1].sizeimage = align_h * align_w;
		break;
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
		/* YUV 422 1planes. */
		*nplanes = 1;
		align_w = w;
		align_h = h;
		/* Y plane */
		plane_fmt[0].bytesperline = align_w * 2;
		plane_fmt[0].sizeimage = align_w * align_h * 2;
		break;
	case V4L2_PIX_FMT_RGB565:
		*nplanes = 1;
		align_w = w;
		align_h = h;
		plane_fmt[0].bytesperline = align_w * 2;
		plane_fmt[0].sizeimage = align_w * align_h * 2;
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
		*nplanes = 1;
		align_w = w;
		align_h = h;
		plane_fmt[0].bytesperline = align_w * 3;
		plane_fmt[0].sizeimage = align_w * align_h * 3;
		break;
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
		*nplanes = 1;
		align_w = w;
		align_h = h;
		plane_fmt[0].bytesperline = align_w * 4;
		plane_fmt[0].sizeimage = align_w * align_h * 4;
		break;
	default:
		*nplanes = 1;
		align_w = w;
		align_h = h;
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_h * align_w;
		break;
	}

	return 0;
}


/*******************************v4l2 device***************************/

int v4l2_device_s_fmt(v4l2_device_t *device)
{
	int i;
	unsigned int nplanes;
	struct v4l2_format *fmt;
	struct v4l2_buffer_info *buf_info;

	/* for output buffer */
	buf_info = &device->out_buff;
	if (buf_info->count > 0) {
		fmt = &buf_info->fmt;
		fmt->type = buf_info->buffer_type;

		if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
			v4l2_fix_planefmt(buf_info->pix_format,
								buf_info->width, buf_info->height,
									fmt->fmt.pix_mp.plane_fmt, &nplanes, 1);
			fmt->fmt.pix_mp.pixelformat = buf_info->pix_format;
			fmt->fmt.pix_mp.field = buf_info->pix_field;
			fmt->fmt.pix_mp.width = buf_info->width;
			fmt->fmt.pix_mp.height = buf_info->height;
			fmt->fmt.pix_mp.num_planes = nplanes;
		} else {
			fmt->fmt.pix.pixelformat = buf_info->pix_format;
			fmt->fmt.pix.field = buf_info->pix_field;
			fmt->fmt.pix.width = buf_info->width;
			fmt->fmt.pix.height = buf_info->height;
		}
		if (v4l2_s_fmt(device->fd, fmt) != 0)
			return -1;

		if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
			buf_info->length = fmt->fmt.pix_mp.num_planes;
			buf_info->sizeimage = 0;
			for (i = 0; i < fmt->fmt.pix_mp.num_planes; i++)
				buf_info->sizeimage += fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
		} else {
			buf_info->length = fmt->fmt.pix.sizeimage;
			buf_info->sizeimage = fmt->fmt.pix.sizeimage;
		}
	}

	/* for capture buffer */
	buf_info = &device->cap_buff;
	if (buf_info->count > 0) {
		fmt = &buf_info->fmt;
		fmt->type = buf_info->buffer_type;

		if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
			v4l2_fix_planefmt(buf_info->pix_format,
								buf_info->width, buf_info->height,
									fmt->fmt.pix_mp.plane_fmt, &nplanes, 1);
			fmt->fmt.pix_mp.pixelformat = buf_info->pix_format;
			fmt->fmt.pix_mp.field = buf_info->pix_field;
			fmt->fmt.pix_mp.width = buf_info->width;
			fmt->fmt.pix_mp.height = buf_info->height;
			fmt->fmt.pix_mp.num_planes = nplanes;
		} else {
			fmt->fmt.pix.pixelformat = buf_info->pix_format;
			fmt->fmt.pix.field = buf_info->pix_field;
			fmt->fmt.pix.width = buf_info->width;
			fmt->fmt.pix.height = buf_info->height;
		}
		if (v4l2_s_fmt(device->fd, fmt) != 0)
			return -1;

		if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
			buf_info->length = fmt->fmt.pix_mp.num_planes;
			buf_info->sizeimage = 0;
			for (i = 0; i < fmt->fmt.pix_mp.num_planes; i++)
				buf_info->sizeimage += fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
		} else {
			buf_info->length = fmt->fmt.pix.sizeimage;
			buf_info->sizeimage = fmt->fmt.pix.sizeimage;
		}
	}

	return 0;
}

int v4l2_device_init(v4l2_device_t *device)
{
	int ret;

	do {
		if (!device->name) {
			LOG_ERR("null device->name");
			return -1;
		}
		ret = v4l2_open(device->name, &device->fd);
		if (ret != 0) {
			LOG_VERBOSE(10, "v4l2_open error, will retry");
			usleep(10000);
		}
	} while (ret != 0);
	MUTEX_INIT(device->mutex_deinit);

	//V4L2_CHECK(ret, device, "v4l2_open error");
	ret = v4l2_device_s_fmt(device); // must before create_buffer, becuase may change plane count
	V4L2_CHECK(ret, device, "v4l2_device_s_fmt error");
	ret = v4l2_create_buffer(device->fd, &device->out_buff);
	V4L2_CHECK(ret, device, "v4l2_create_buffer out_buff error");
	ret = v4l2_create_buffer(device->fd, &device->cap_buff);
	V4L2_CHECK(ret, device, "v4l2_create_buffer cap_buff error");

	return 0;
}

int v4l2_device_deinit(v4l2_device_t *device)
{
	MUTEX_LOCK(device->mutex_deinit);
	v4l2_device_deqbuf_all(device,V4L2_BUFF_OUTPUT);
	v4l2_destroy_buffer(device->fd, &device->out_buff);
	v4l2_device_deqbuf_all(device,V4L2_BUFF_CAPTURE);
	v4l2_destroy_buffer(device->fd, &device->cap_buff);
	v4l2_close(&device->fd);
	MUTEX_UNLOCK(device->mutex_deinit);
	return 0;
}

struct v4l2_buffer* v4l2_device_deqbuf(v4l2_device_t *device, int type)
{
	struct v4l2_buffer_info *buf_info;
	struct v4l2_buffer buffer;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];

	if (type == V4L2_BUFF_OUTPUT) {
		buf_info = &device->out_buff;
	} else {
		buf_info = &device->cap_buff;
	}

	memset(&buffer, 0, sizeof(buffer));
	memset(&planes, 0, sizeof(planes));

	buffer.type = buf_info->buffer_type;
	buffer.memory = buf_info->memory_type;
	if (V4L2_TYPE_IS_MULTIPLANAR(buf_info->buffer_type)) {
		buffer.m.planes = planes;
		buffer.length = buf_info->length;
	}

	MUTEX_LOCK(buf_info->mutex_stat);
	if (0 != v4l2_deqbuf(device->fd, &buffer)) {
		MUTEX_UNLOCK(buf_info->mutex_stat);
		return NULL;
	}
	buf_info->buff_stat[buffer.index] = V4L2_BUFF_STAT_DEQUEUED;
	MUTEX_UNLOCK(buf_info->mutex_stat);

	LOG_VERBOSE(1000, "%s deq(%d)",
		V4L2_TYPE_IS_OUTPUT(buffer.type) ? "output" : "capture",
		buffer.index);

	return &buf_info->buffer[buffer.index];
}

int v4l2_device_deqbuf_all(v4l2_device_t *device, int type)
{
	struct v4l2_buffer_info *buf_info;
	struct v4l2_buffer *buffer;

	if (type == V4L2_BUFF_OUTPUT) {
		buf_info = &device->out_buff;
	} else {
		buf_info = &device->cap_buff;
	}
	if (buf_info->count <= 0)
		return 0;

	do buffer = v4l2_device_deqbuf(device, type);
	while (buffer);

	return 0;
}

int v4l2_device_qbuf(v4l2_device_t *device, struct v4l2_buffer *buffer)
{
	int ret;
	struct v4l2_buffer_info *buf_info;

	if (V4L2_TYPE_IS_OUTPUT(buffer->type)) {
		buf_info = &device->out_buff;
	} else {
		buf_info = &device->cap_buff;
	}

	MUTEX_LOCK(buf_info->mutex_stat);
	ret = v4l2_qbuf(device->fd, buffer);
	if (0 != ret ) {
		LOG_INFO("V4L2_qbuf errno is %s",strerror(errno));
		qbuf_error_flag = VIDEO_QBUF_ERROR;
		MUTEX_UNLOCK(buf_info->mutex_stat);
		return 0;
	}
	buf_info->buff_stat[buffer->index] = V4L2_BUFF_STAT_QUEUED;
	MUTEX_UNLOCK(buf_info->mutex_stat);

	LOG_VERBOSE(1000, "%s enq(%d)",
		V4L2_TYPE_IS_OUTPUT(buffer->type) ? "output" : "capture",
		buffer->index);

	if (!buf_info->stream_on) {
		if (0 == v4l2_streamon(device->fd, buf_info->buffer_type))
			buf_info->stream_on = 1;
	}

	return 0;
}

int v4l2_device_qbuf_all(v4l2_device_t *device, int type)
{
	struct v4l2_buffer_info *buf_info;

	if (type == V4L2_BUFF_OUTPUT) {
		buf_info = &device->out_buff;
	} else {
		buf_info = &device->cap_buff;
	}

	int i;
	for (i = 0; i < buf_info->count; i++) {
		if (buf_info->buff_stat[i] == V4L2_BUFF_STAT_FREE)
			v4l2_device_qbuf(device, &buf_info->buffer[i]);
	}

	return 0;
}

bool v4l2_device_has_free_buffer(v4l2_device_t *device, int type)
{
	int i;
	struct v4l2_buffer_info *buf_info;

	if (type == V4L2_BUFF_OUTPUT) {
		buf_info = &device->out_buff;
	} else {
		buf_info = &device->cap_buff;
	}

	for(i = 0; i < buf_info->count; i++) {
		if (buf_info->buff_stat[i] == V4L2_BUFF_STAT_FREE ||
			buf_info->buff_stat[i] == V4L2_BUFF_STAT_DEQUEUED) {
			return true;
		}
	}
	return false;
}

int v4l2_device_queue_usrptr(v4l2_device_t *device, int type, void *ptr)
{
	int i;
	struct v4l2_buffer *buffer;
	struct v4l2_buffer_info *buf_info;

	if (type == V4L2_BUFF_OUTPUT) {
		buf_info = &device->out_buff;
		LOG_VERBOSE(1000, "output");
	} else {
		buf_info = &device->cap_buff;
		LOG_VERBOSE(1000, "capture");
	}

	buffer = NULL;
	MUTEX_LOCK(buf_info->mutex_stat);
	for(i = 0; i < buf_info->count; i++) {
		if (buf_info->buff_stat[i] == V4L2_BUFF_STAT_FREE) {
			buf_info->buff_stat[i] = V4L2_BUFF_STAT_BUSY;
			buffer = &buf_info->buffer[i];
			break;
		} else if ((buf_info->memory_type == V4L2_MEMORY_MMAP)
				&& (buf_info->buff_stat[i] == V4L2_BUFF_STAT_DEQUEUED)) {
			buf_info->buff_stat[i] = V4L2_BUFF_STAT_BUSY;
			buffer = &buf_info->buffer[i];
			break;
		}
	}
	MUTEX_UNLOCK(buf_info->mutex_stat);

	if (!buffer) {
		LOG_VERBOSE(10, "No available %s buffer",
			type == V4L2_BUFF_OUTPUT ? "output" : "capture");
		return -1;
	}

	if (buffer->memory == V4L2_MEMORY_USERPTR) {
		if (V4L2_TYPE_IS_MULTIPLANAR(buffer->type)) {
			int i;
			void **va_list;
			//LOG_DBG("ptr[0x%llx] 1", (unsigned long)ptr);
			va_list = (void**)ptr;
			buf_info->va[buffer->index].va_list = va_list;

			for (i = 0; i < buffer->length; i++) {
				buffer->m.planes[i].m.userptr = (unsigned long)va_list[i];
			}
		} else {
			//LOG_DBG("ptr[0x%llx] 2", (unsigned long)ptr);
			buffer->m.userptr = (unsigned long)ptr;
			buf_info->va[buffer->index].va = ptr;
		}
	} else if (type == V4L2_BUFF_OUTPUT &&
					buffer->memory == V4L2_MEMORY_MMAP) {
		if (V4L2_TYPE_IS_MULTIPLANAR(buffer->type)) {
			int i;
			void **va_list;
			void **ptr_list;
			//LOG_DBG("ptr[0x%llx] 3", (unsigned long)ptr);

			ptr_list = (void**)ptr;
			va_list = buf_info->va[buffer->index].va_list;

			for (i = 0; i < buffer->length; i++) {
				memcpy(va_list[i], ptr_list[i], buffer->m.planes[i].length);
			}
		} else {
			memcpy(buf_info->va[buffer->index].va, ptr, buffer->length);
		}
	} else {
		// will never happen
		V4L2_DEV_ERR(device, "Error buffer_type & memory_type");
		return -1;
	}
	//LOG_DBG("ptr[0x%llx]", (unsigned long)ptr);

	V4L2_UNTILL_SUCCESS(v4l2_device_qbuf(device, buffer));
	return 0;
}

/*******************************module driver**************************/

int v4l2_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_SELF_OTHERS;
			break;
		default:
			return -1;
	}
	return 0;
}

void* v4l2_mem_alloc(void *private, int type, int *size)
{
	struct v4l2_device *dev;
	struct v4l2_buffer *buffer;
	struct v4l2_buffer_info *buf_info;
	dev = (v4l2_device*)private;

	if (type == BUFFER_INPUT) {
		buf_info = &dev->out_buff;
	} else {
		buf_info = &dev->cap_buff;
	}
	if (!buf_info->buffer)
		return NULL;

	MUTEX_LOCK(buf_info->mutex_stat);
	int i;
	buffer = NULL;
	for (i = 0; i < buf_info->count; i++) {
		if (buf_info->buff_stat[i] == V4L2_BUFF_STAT_FREE) {
			buffer = &buf_info->buffer[i];
			buf_info->buff_stat[i] = V4L2_BUFF_STAT_BUSY;
			break;
		}
	}
	MUTEX_UNLOCK(buf_info->mutex_stat);

	if (buffer)
		return buffer;

	/* dequeue buffer */
	if (type == BUFFER_INPUT) {
		buffer = v4l2_device_deqbuf(dev, V4L2_BUFF_OUTPUT);
	} else {
		buffer = v4l2_device_deqbuf(dev, V4L2_BUFF_CAPTURE);
	}

	if (buffer) {
		/* fix size */
		if (V4L2_TYPE_IS_MULTIPLANAR(buffer->type)) {
			int i;
			*size = 0;
			for (i = 0; i < buffer->length; i++)
				*size += buffer->m.planes[i].length;
		} else {
			*size = buffer->length;
		}

		/* change status */
		if (V4L2_TYPE_IS_OUTPUT(buffer->type)) {
			buf_info = &dev->out_buff;
		} else {
			buf_info = &dev->cap_buff;
		}

		MUTEX_LOCK(buf_info->mutex_stat);
		buf_info->buff_stat[buffer->index] = V4L2_BUFF_STAT_BUSY;
		MUTEX_UNLOCK(buf_info->mutex_stat);

		LOG_VERBOSE(10000, "alloc(%d) %p (%d)", *size, buffer, buffer->index);
	}

	return (void*)buffer;
}

int v4l2_mem_free(void *private, void *buff)
{
	struct v4l2_device *dev;
	dev= (v4l2_device*)private;
	struct v4l2_buffer *buf;

	MUTEX_LOCK(dev->mutex_deinit);
	buf = (struct v4l2_buffer*)buff;
	if (buff == NULL){
		LOG_WARN("buffer is NULL");
		MUTEX_UNLOCK(dev->mutex_deinit);
		return 0;
	}
	if (dev->fd == -1){
		LOG_WARN("device is closed, return");
		MUTEX_UNLOCK(dev->mutex_deinit);
		return 0;
	}
	V4L2_UNTILL_SUCCESS(v4l2_device_qbuf(dev, buf));
	MUTEX_UNLOCK(dev->mutex_deinit);
	LOG_VERBOSE(10000, "free(%d) %p (%d)", buf->index, buf, buf->index);
	return 0;
}

void* v4l2_mem_get_ptr(void *private, void *buff)
{
	struct v4l2_device *dev;
	dev = (v4l2_device*)private;

	struct v4l2_buffer *buf;
	buf = (struct v4l2_buffer*)buff;

	struct v4l2_buffer_info *buf_info;
	if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
		buf_info = &dev->out_buff;
	} else {
		buf_info = &dev->cap_buff;
	}

	if (buf->index >= buf_info->count)
		return NULL;

	return (void*)buf_info->va[buf->index].va_list;
}

memory_ops v4l2_mem_ops = {
	.caps = v4l2_mem_caps,
	.alloc = v4l2_mem_alloc,
	.free = v4l2_mem_free,
	.get_ptr = v4l2_mem_get_ptr,
};

/* this function should be used when V4L2_MEMORY_USERPTR */
int v4l2_release_input_frame(module *mod)
{
	LOG_VERBOSE(10000, "Enter");
	int i;
	struct v4l2_device *dev;
	struct v4l2_buffer_info *buf_info;
	shm_buff *shm;

	dev = (v4l2_device*)module_get_driver_data(mod);
	v4l2_device_deqbuf(dev, V4L2_BUFF_OUTPUT);

	shm = (shm_buff*)queue_deq(dev->out_buff.waiting_queue);
	if (!shm)
		return 0;
	void *ptr = shm_get_ptr(shm);

	buf_info = &dev->out_buff;
	MUTEX_LOCK(buf_info->mutex_stat);
	for(i = 0; i < buf_info->count; i++) {
		if (buf_info->buff_stat[i] == V4L2_BUFF_STAT_DEQUEUED
				&& buf_info->va[i].va == ptr) {
			buf_info->buff_stat[i] = V4L2_BUFF_STAT_FREE;
			MUTEX_UNLOCK(buf_info->mutex_stat);

			shm_free(shm);
			LOG_VERBOSE(1000, "Done");
			return 0;
		}
	}
	MUTEX_UNLOCK(buf_info->mutex_stat);
	queue_enq_head(dev->out_buff.waiting_queue, shm);

	LOG_VERBOSE(10000, "Leave");
	return 0;
}

/* this function should be used when V4L2_MEMORY_USERPTR */
int v4l2_push_output_frame(module *mod)
{
	LOG_VERBOSE(10000, "Enter");
	int i;
	struct v4l2_device *dev;
	struct v4l2_buffer_info *buf_info;
	shm_buff *shm;

	dev = (v4l2_device*)module_get_driver_data(mod);
	v4l2_device_deqbuf(dev, V4L2_BUFF_CAPTURE);

	shm = (shm_buff*)queue_deq(dev->cap_buff.waiting_queue);
	if (!shm)
		return 0;
	void *ptr = shm_get_ptr(shm);

	buf_info = &dev->cap_buff;
	MUTEX_LOCK(buf_info->mutex_stat);
	for(i = 0; i < buf_info->count; i++) {
		if (buf_info->buff_stat[i] == V4L2_BUFF_STAT_DEQUEUED
				&& buf_info->va[i].va_list == (void**)ptr) {
			buf_info->buff_stat[i] = V4L2_BUFF_STAT_FREE;
			MUTEX_UNLOCK(buf_info->mutex_stat);

			module_push_frame(mod, shm);
			LOG_VERBOSE(1000, "Done");
			return 0;
		}
	}
	MUTEX_UNLOCK(buf_info->mutex_stat);
	queue_enq_head(dev->cap_buff.waiting_queue, shm);

	LOG_VERBOSE(10000, "Leave");
	return 0;
}

void v4l2_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct v4l2_device *dev;
	dev = (v4l2_device*)module_get_driver_data(mod);

	if (strcmp(name, "name") == 0) {
		dev->name = strdup((char*)value);
	}
	/* output buffer configs */
	else if (strstr(name, "out_buff")) {
		if (strcmp(name, "out_buff.width") == 0) {
			dev->out_buff.width = value;
		} else if (strcmp(name, "out_buff.height") == 0) {
			dev->out_buff.height = value;
		} else if (strcmp(name, "out_buff.length") == 0) {
			dev->out_buff.length = value;
		} else if (strcmp(name, "out_buff.count") == 0) {
			dev->out_buff.count = value;
		} else if (strcmp(name, "out_buff.buffer_type") == 0) {
			dev->out_buff.buffer_type = value;
		} else if (strcmp(name, "out_buff.memory_type") == 0) {
			dev->out_buff.memory_type = value;
		} else if (strcmp(name, "out_buff.pix_format") == 0) {
			dev->out_buff.pix_format = value;
		} else if (strcmp(name, "out_buff.pix_field") == 0) {
			dev->out_buff.pix_field = value;
		} else if (strcmp(name, "out_buff.rotation") == 0) {
			dev->out_buff.rotation = value;
		} else if (strcmp(name, "out_buff.crop.c.top") == 0) {
			dev->out_buff.crop.c.top = value;
		} else if (strcmp(name, "out_buff.crop.c.left") == 0) {
			dev->out_buff.crop.c.left = value;
		} else if (strcmp(name, "out_buff.crop.c.width") == 0) {
			dev->out_buff.crop.c.width = value;
		} else if (strcmp(name, "out_buff.crop.c.height") == 0) {
			dev->out_buff.crop.c.height = value;
		}
	}
	/* capture buffer configs */
	else if (strstr(name, "cap_buff")) {
		if (strcmp(name, "cap_buff.width") == 0) {
			dev->cap_buff.width = value;
		} else if (strcmp(name, "cap_buff.height") == 0) {
			dev->cap_buff.height = value;
		} else if (strcmp(name, "cap_buff.length") == 0) {
			dev->cap_buff.length= value;
		} else if (strcmp(name, "cap_buff.count") == 0) {
			dev->cap_buff.count = value;
		} else if (strcmp(name, "cap_buff.buffer_type") == 0) {
			dev->cap_buff.buffer_type = value;
		} else if (strcmp(name, "cap_buff.memory_type") == 0) {
			dev->cap_buff.memory_type = value;
		} else if (strcmp(name, "cap_buff.pix_format") == 0) {
			dev->cap_buff.pix_format = value;
		} else if (strcmp(name, "cap_buff.pix_field") == 0) {
			dev->cap_buff.pix_field = value;
		} else if (strcmp(name, "cap_buff.rotation") == 0) {
			dev->cap_buff.rotation = value;
		} else if (strcmp(name, "cap_buff.crop.c.top") == 0) {
			dev->cap_buff.crop.c.top = value;
		} else if (strcmp(name, "cap_buff.crop.c.left") == 0) {
			dev->cap_buff.crop.c.left = value;
		} else if (strcmp(name, "cap_buff.crop.c.width") == 0) {
			dev->cap_buff.crop.c.width = value;
		} else if (strcmp(name, "cap_buff.crop.c.height") == 0) {
			dev->cap_buff.crop.c.height = value;
		}
	}
}

int v4l2_driver_init(struct module *mod)
{
	LOG_DBG("Enter");
	struct v4l2_device *dev;
	dev = (v4l2_device*)module_get_driver_data(mod);

	// fix memory type
	int type;
	type = module_get_memory_type(mod, CAPS_MEM_OUTPUT);
	if (type == MEMORY_ALLOC_SELF) {
		dev->cap_buff.memory_type = V4L2_MEMORY_MMAP;
	} else {
		dev->cap_buff.memory_type = V4L2_MEMORY_USERPTR;
		dev->cap_buff.waiting_queue= queue_create();
	}
	type = module_get_memory_type(mod, CAPS_MEM_INPUT);
	if (type == MEMORY_ALLOC_SELF) {
		dev->out_buff.memory_type = V4L2_MEMORY_MMAP;
	} else {
		dev->out_buff.memory_type = V4L2_MEMORY_USERPTR;
		dev->out_buff.waiting_queue= queue_create();
	}

	// do initialize
	if (v4l2_device_init(dev) != 0)
		return -1;

	// queue buffer
	//type = module_get_memory_type(mod, CAPS_MEM_INPUT);
	//if (type == MEMORY_ALLOC_SELF) {
	//	v4l2_device_qbuf_all(dev, V4L2_BUFF_OUTPUT);
	//}
	type = module_get_memory_type(mod, CAPS_MEM_OUTPUT);
	if (type == MEMORY_ALLOC_SELF) {
		v4l2_device_qbuf_all(dev, V4L2_BUFF_CAPTURE);
	}

	LOG_DBG("Leave");
	return 0;
}

void v4l2_driver_deinit(struct module *mod)
{
	LOG_DBG("Enter");
	struct v4l2_device *dev;
	struct shm_buff *shm;
	dev = (v4l2_device*)module_get_driver_data(mod);
	if (dev->out_buff.waiting_queue) {
		while (shm = queue_deq(dev->out_buff.waiting_queue))
			shm_free(shm);
		queue_destroy(dev->out_buff.waiting_queue);
		dev->out_buff.waiting_queue = NULL;
	}
	if (dev->cap_buff.waiting_queue) {
		while (shm = queue_deq(dev->cap_buff.waiting_queue))
			shm_free(shm);
		queue_destroy(dev->cap_buff.waiting_queue);
		dev->cap_buff.waiting_queue = NULL;
	}
	v4l2_device_deinit(dev);
	LOG_DBG("Leave");
}

int v4l2_driver_handle_frame(struct module *mod)
{
	LOG_VERBOSE(300000, "Enter");
	int ret;
	struct v4l2_device *dev;
	struct shm_buff *in_frame, *out_frame;
	dev = (v4l2_device*)module_get_driver_data(mod);

	if (dev->out_buff.waiting_queue)
		v4l2_release_input_frame(mod);
	if (dev->cap_buff.waiting_queue)
		v4l2_push_output_frame(mod);
	/*
	* queue input frame
	*/
	//MOD_DBG(mod, "dev->out_buff.memory_type is %d", dev->out_buff.memory_type);
	//MOD_DBG(mod, "dev->cap_buff.memory_type is %d", dev->cap_buff.memory_type);
	in_frame = module_get_frame(mod);
	if (in_frame) {
		LOG_VERBOSE(1000, "get frame");
		if (module_if_ours_buffer(mod, in_frame)) {
			shm_free(in_frame); // free will enqueue frame
		} else if (dev->out_buff.memory_type == V4L2_MEMORY_USERPTR) {
			ret = v4l2_device_queue_usrptr(dev, V4L2_BUFF_OUTPUT, shm_get_ptr(in_frame));
			if (qbuf_error_flag == VIDEO_QBUF_SUCCESS) {
				if (ret < 0)
					shm_free(in_frame);
				else
					queue_enq(dev->out_buff.waiting_queue, in_frame);
			} else {
				shm_free(in_frame);
				LOG_INFO("qbuf_error_flag =  %d",qbuf_error_flag);
				qbuf_error_flag = VIDEO_QBUF_SUCCESS;
			}
		} else if (dev->out_buff.memory_type == V4L2_MEMORY_MMAP) {
			v4l2_device_deqbuf_all(dev, V4L2_BUFF_OUTPUT);
			v4l2_device_queue_usrptr(dev, V4L2_BUFF_OUTPUT, shm_get_ptr(in_frame));
			shm_free(in_frame);
		} else {
			MOD_ERR(mod, "wrong memory type %d", dev->out_buff.memory_type);
			shm_free(in_frame);
		}
	}

	/*
	* push output frame
	*/
	if (module_get_memory_type(mod, CAPS_MEM_OUTPUT) == MEMORY_ALLOC_OTHERS) {
		if (v4l2_device_has_free_buffer(dev, V4L2_BUFF_CAPTURE)) {
			out_frame = module_next_alloc_buffer(mod, dev->cap_buff.sizeimage);
			if (out_frame) {
				ret = v4l2_device_queue_usrptr(dev, V4L2_BUFF_CAPTURE, shm_get_ptr(out_frame));
				if (ret < 0)
					shm_free(out_frame);
				else
					queue_enq(dev->cap_buff.waiting_queue, out_frame);
			}
		}
	} else {
		out_frame = module_alloc_buffer(mod,
						BUFFER_OUTPUT, dev->cap_buff.sizeimage); //alloc will deque buff
		if (out_frame) {
			module_push_frame(mod, out_frame);
		}
	}

	LOG_VERBOSE(300000, "Leave");
	return 0;
}

module_driver v4l2_driver = {
	.config = v4l2_driver_config,
	.init = v4l2_driver_init,
	.deinit = v4l2_driver_deinit,
	.handle_frame = v4l2_driver_handle_frame,
};

int v4l2_module_init(module *mod)
{
	struct v4l2_device *dev;
	dev = (v4l2_device*)malloc(sizeof(v4l2_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "v4l2");
	module_register_driver(mod, &v4l2_driver, dev);
	module_register_allocator(mod, &v4l2_mem_ops, dev);
	return 0;
}
