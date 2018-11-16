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

static int v4l2_codec_mem_caps(void *private, int cap, int *val)
{
	// decode need too much input/output buffer, so must alloc self
	switch (cap) {
		case CAPS_MEM_INPUT:
			*val = MEMORY_ALLOC_SELF;
			break;
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_SELF;
			break;
		default:
			return -1;
	}
	return 0;
}

static void* v4l2_codec_mem_get_ptr(void *private, void *buff)
{
	struct v4l2_device *dev;
	dev = (v4l2_device*)private;

	struct v4l2_buffer *buf;
	buf = (struct v4l2_buffer*)buff;

	struct v4l2_buffer_info *buf_info;
	if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
		buf_info = &dev->out_buff;
	} else {
		return v4l2_mem_get_ptr(private, buff);
	}

	if (buf->index >= buf_info->count)
		return NULL;

	if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
		return (void*)buf_info->va[buf->index].va_list[0];
	} else {
		return (void*)buf_info->va[buf->index].va;
	}
}

memory_ops v4l2_codec_mem_ops = {
	.caps = v4l2_codec_mem_caps,
	.alloc = v4l2_mem_alloc,
	.free = v4l2_mem_free,
	.get_ptr = v4l2_codec_mem_get_ptr,
};

static int v4l2_codec_device_init(v4l2_device_t *device)
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
			usleep(100000);
		}
	} while (ret != 0);

	//V4L2_CHECK(ret, device, "v4l2_open error");
	ret = v4l2_device_s_fmt(device); // must before create_buffer, becuase may change plane count
	V4L2_CHECK(ret, device, "v4l2_device_s_fmt error");
	ret = v4l2_create_buffer(device->fd, &device->out_buff);
	V4L2_CHECK(ret, device, "v4l2_create_buffer out_buff error");
	//ret = v4l2_create_buffer(device->fd, &device->cap_buff);
	//V4L2_CHECK(ret, device, "v4l2_create_buffer cap_buff error");

	return 0;
}

static int v4l2_codec_driver_init(struct module *mod)
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
	}
	type = module_get_memory_type(mod, CAPS_MEM_INPUT);
	if (type == MEMORY_ALLOC_SELF) {
		dev->out_buff.memory_type = V4L2_MEMORY_MMAP;
	} else {
		dev->out_buff.memory_type = V4L2_MEMORY_USERPTR;
	}

	// do initialize
	if (v4l2_codec_device_init(dev) != 0)
		return -1;

	LOG_DBG("Leave");
	return 0;
}

static int v4l2_codec_driver_handle_frame(struct module *mod)
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
	in_frame = module_get_frame(mod);
	if (in_frame) {
		if (module_if_ours_buffer(mod, in_frame)) {
			/* fix byteused */
			struct v4l2_buffer *buf;
			buf = (struct v4l2_buffer*)in_frame->buff;
			if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
				buf->m.planes[0].bytesused = in_frame->size;
			} else {
				buf->bytesused = in_frame->size;
			}
			// queue buffer
			shm_free(in_frame);
		} else if (dev->out_buff.memory_type == V4L2_MEMORY_USERPTR) {
			ret = v4l2_device_queue_usrptr(dev, V4L2_BUFF_OUTPUT, shm_get_ptr(in_frame));
			if (ret < 0)
				shm_free(in_frame);
			else
				queue_enq(dev->out_buff.waiting_queue, in_frame);
		} else if (dev->out_buff.memory_type == V4L2_MEMORY_MMAP) {
			v4l2_device_deqbuf_all(dev, V4L2_BUFF_OUTPUT);
			v4l2_device_queue_usrptr(dev, V4L2_BUFF_OUTPUT, shm_get_ptr(in_frame));
			shm_free(in_frame);
		} else {
			MOD_ERR(mod, "wrong memory type %d", dev->out_buff.memory_type);
			shm_free(in_frame);
		}

		// create capture buffer after we send frame
		if (dev->cap_buff.buffer == NULL) {
			int type;
			v4l2_g_fmt(dev->fd, &dev->cap_buff.fmt);
			v4l2_create_buffer(dev->fd, &dev->cap_buff);
			if (dev->cap_buff.buffer) {
				type = module_get_memory_type(mod, CAPS_MEM_OUTPUT);
				if (type == MEMORY_ALLOC_SELF) {
					v4l2_device_qbuf_all(dev, V4L2_BUFF_CAPTURE);
				}
			} else {
				LOG_ERR("capture buffer create fail");
				return 0;
			}
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
						BUFFER_OUTPUT, 0); //alloc will deque buff
		if (out_frame) {
			module_push_frame(mod, out_frame);
		}
	}

	LOG_VERBOSE(300000, "Leave");
	return 0;
}


static module_driver v4l2_codec_driver = {
	.config = v4l2_driver_config,
	.init = v4l2_codec_driver_init,
	.deinit = v4l2_driver_deinit,
	.handle_frame = v4l2_codec_driver_handle_frame,
};

int v4l2_codec_module_init(module *mod)
{
	struct v4l2_device *dev;
	dev = (v4l2_device*)malloc(sizeof(v4l2_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "v4l2_codec");
	module_register_driver(mod, &v4l2_codec_driver, dev);
	module_register_allocator(mod, &v4l2_codec_mem_ops, dev);
	return 0;
}
