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
#include "mtk_ovl_adapter.h"
#include "videodev2.h"
#include "module_v4l2.h"
#include "cpp_util.h"
#include "FastrvcDef.h"

extern int mtk_ovl_adapter_init(
	void **p_handle, struct MTK_OVL_ADAPTER_PARAM_INIT *param);
extern int mtk_ovl_adapter_work(
	void *handle, struct MTK_OVL_ADAPTER_PARAM_WORK *param);
extern int mtk_ovl_adapter_uninit(void *handle);
extern int mtk_ovl_test_io(unsigned int is_read, char *path, void *addr, unsigned int req_size);

#define BUF_COUNT 3
#define ALLOC_ALIGN 4096

#define ALIGN(v, m) (((v) + (m) - 1) / (m) * (m))
#define PATH_ARRAY_SIZE 256
#define DRM_BUFF 1

struct BUF_ADDR {
	unsigned long va_alloc;
	unsigned long va_align;
	unsigned int buf_id;
	unsigned long size;
};
typedef struct ovl_buffer {
	struct shm_buff *shm;
	void **va_list;
} ovl_buffer;

typedef struct ovl_device {
	char	*guildeline_file;
	FILE	*fd;
	struct v4l2_plane_pix_format plane_info[VIDEO_MAX_PLANES];
	struct MTK_OVL_ADAPTER_PARAM_INIT param_init;
	struct MTK_OVL_ADAPTER_PARAM_WORK param_work;
	unsigned int	length;
	void	*ptr;
	void 	*handle;
	unsigned int     num_planes;
	struct BUF_ADDR input_buf_addr[INPUT_DEV_COUNT][BUF_COUNT];
	struct BUF_ADDR output_buf_addr[BUF_COUNT];
	int buffer_status[BUF_COUNT];
	MUTEX_HANDLE(mutex_buffer);
} ovl_device;

/*******************************module driver**************************/

int mtk_ovl_test_alloc_buf(struct BUF_ADDR *buf_addr, unsigned int buf_size, unsigned int buf_align)
{
	int ret = 0;

	memset(buf_addr, 0, sizeof(buf_addr));

	buf_addr->va_alloc = (unsigned long)malloc(ALIGN(buf_size, 4096));
	buf_addr->va_align = ALIGN(buf_addr->va_alloc, 4096);
	buf_addr->size = buf_size;

	if (buf_addr->va_alloc && buf_addr->va_align) {
		LOG_DBG("ok to alloc buf, addr[0x%llx, 0x%llx], size[%d], align[%d]",
			buf_addr->va_alloc, buf_addr->va_align,
			buf_size, buf_align);
	}
	else {
		LOG_DBG("fail to alloc buf, addr[0x%llx, 0x%llx], size[%d], align[%d]",
			buf_addr->va_alloc, buf_addr->va_align,
			buf_size, buf_align);
		ret = 1;
	}

	return ret;
}

int mtk_ovl_test_free_buf(struct BUF_ADDR *buf_addr)
{
	int ret = 0;

	LOG_DBG("will to free buf, addr[0x%llx, 0x%llx]",
		buf_addr->va_alloc, buf_addr->va_align);

	free((void*)(buf_addr->va_alloc));

	return ret;
}
void dump_buffer_info(struct ovl_device *dev)
{
	int dev_idx = 0;
	int buf_idx = 0;
	int ret;
	/* free buffer */
	for (dev_idx = 0; dev_idx < INPUT_DEV_COUNT; dev_idx++) {
		for (buf_idx = 0; buf_idx < dev->param_init.input_buf[dev_idx].buf_cnt; buf_idx++) {
			LOG_DBG("dev->input_buf_addr[%d][%d].va_alloc=0x%llx", dev_idx, buf_idx, dev->input_buf_addr[dev_idx][buf_idx].va_alloc);
			LOG_DBG("dev->input_buf_addr[%d][%d].va_align=0x%llx", dev_idx, buf_idx, dev->input_buf_addr[dev_idx][buf_idx].va_align);
			LOG_DBG("dev->input_buf_addr[%d][%d].buf_id=%d", dev_idx, buf_idx, dev->input_buf_addr[dev_idx][buf_idx].buf_id);
			LOG_DBG("dev->input_buf_addr[%d][%d].size=%d", dev_idx, buf_idx, dev->input_buf_addr[dev_idx][buf_idx].size);
		}
	}

	for (buf_idx = 0; buf_idx < dev->param_init.output_buf.buf_cnt; buf_idx++) {
		LOG_DBG("dev->output_buf_addr[%d].va_alloc=0x%llx", buf_idx, dev->output_buf_addr[buf_idx].va_alloc);
		LOG_DBG("dev->output_buf_addr[%d].va_align=0x%llx", buf_idx, dev->output_buf_addr[buf_idx].va_align);
		LOG_DBG("dev->output_buf_addr[%d].buf_id=%d", buf_idx, dev->output_buf_addr[buf_idx].buf_id);
		LOG_DBG("dev->output_buf_addr[%d].size=%d", buf_idx, dev->output_buf_addr[buf_idx].size);
	}
}

int find_pitch(struct ovl_device *dev)
{
	int dev_idx = 0;
	int buf_idx = 0;
	int ret;
	/* free buffer */
	for (dev_idx = 0; dev_idx < INPUT_DEV_COUNT; dev_idx++) {
		for (buf_idx = 0; buf_idx < dev->param_init.input_buf[dev_idx].buf_cnt; buf_idx++) {
			switch (dev->param_init.input_buf[dev_idx].fmt_4cc) {
				case V4L2_PIX_FMT_UYVY:
				case V4L2_PIX_FMT_YUYV:
				case V4L2_PIX_FMT_YVYU:
				case V4L2_PIX_FMT_VYUY:
					dev->param_init.input_buf[dev_idx].pitch = dev->param_init.input_buf[dev_idx].width * 2;
					break;
				case V4L2_PIX_FMT_ABGR32:
				case V4L2_PIX_FMT_ARGB32:
					dev->param_init.input_buf[dev_idx].pitch = dev->param_init.input_buf[dev_idx].width * 4;
					break;
				default:
					LOG_ERR("unsupport fmt_cc type");
					break;
			}
		}
	}
	for (buf_idx = 0; buf_idx < dev->param_init.input_buf[dev_idx].buf_cnt; buf_idx++) {
		switch (dev->param_init.output_buf.fmt_4cc) {
			case V4L2_PIX_FMT_UYVY:
			case V4L2_PIX_FMT_YUYV:
			case V4L2_PIX_FMT_YVYU:
			case V4L2_PIX_FMT_VYUY:
				dev->param_init.output_buf.pitch = dev->param_init.output_buf.width * 2;
				break;
			case V4L2_PIX_FMT_ABGR32:
			case V4L2_PIX_FMT_ARGB32:
				dev->param_init.output_buf.pitch = dev->param_init.output_buf.width * 4;
				break;
			default:
				LOG_ERR("unsupport fmt_cc type");
				break;
		}
	}
	return 0;
}

static int ovl_mem_caps(void *private, int cap, int *val)
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

static void* ovl_mem_alloc(void *private, int type, int *size)
{
	int i;

	struct ovl_device *dev;
	dev = (ovl_device*)private;

	if (type == BUFFER_INPUT) {
	} else {
		return NULL;
	}

	MUTEX_LOCK(dev->mutex_buffer);
	for (i = 0; i < BUF_COUNT; i++) {
		if (dev->buffer_status[i] == 0) {
			dev->buffer_status[i] = 1;
			MUTEX_UNLOCK(dev->mutex_buffer);
			*size = dev->input_buf_addr[0][i].size;
			return &(dev->input_buf_addr[0][i]);
		}
	}
	MUTEX_UNLOCK(dev->mutex_buffer);

	return NULL;
}

static int ovl_mem_free(void *private, void *buff)
{
	int i;

	struct ovl_device *dev;
	dev= (ovl_device*)private;

	MUTEX_LOCK(dev->mutex_buffer);
	for (i = 0; i < BUF_COUNT; i++) {
		if (&dev->input_buf_addr[0][i] == buff) {
			dev->buffer_status[i] = 0;
			break;
		}
	}
	MUTEX_UNLOCK(dev->mutex_buffer);
	return 0;
}

static void* ovl_mem_get_ptr(void *private, void *buff)
{
	return (void *)((struct BUF_ADDR*)buff)->va_align;
}

static memory_ops ovl_mem_ops = {
	.caps = ovl_mem_caps,
	.alloc = ovl_mem_alloc,
	.free = ovl_mem_free,
	.get_ptr = ovl_mem_get_ptr,
};

static void ovl_driver_config(struct module *mod, char *name, unsigned long value)
{
	struct ovl_device *dev;
	dev = (ovl_device*)module_get_driver_data(mod);

	if (strcmp(name, "guideline_file") == 0) {
		dev->guildeline_file = strdup((char*)value);
	/*ovl0 inputbuf setting */
	} else if (strcmp(name, "ovl0.enable") == 0) {
		dev->param_init.input_buf[0].enable = value;
	} else if (strcmp(name, "ovl0.width") == 0) {
		dev->param_init.input_buf[0].width = value;
	} else if (strcmp(name, "ovl0.height") == 0) {
		dev->param_init.input_buf[0].height = value;
	} else if (strcmp(name, "ovl0.pix_format") == 0) {
		dev->param_init.input_buf[0].fmt_4cc = value;
	} else if (strcmp(name, "ovl0.buffer_type") == 0) {
		dev->param_init.input_buf[0].buf_type = value;
	} else if (strcmp(name, "ovl0.buffer_count") == 0) {
		dev->param_init.input_buf[0].buf_cnt = value;
	} else if (strcmp(name, "ovl0.plane_count") == 0) {
		dev->param_init.input_buf[0].plane_cnt = value;
	/*ovl1 inputbuf setting */
	} else if (strcmp(name, "ovl1.enable") == 0) {
		dev->param_init.input_buf[1].enable = value;
	} else if (strcmp(name, "ovl1.width") == 0) {
		dev->param_init.input_buf[1].width = value;
	} else if (strcmp(name, "ovl1.height") == 0) {
		dev->param_init.input_buf[1].height = value;
	} else if (strcmp(name, "ovl1.pix_format") == 0) {
		dev->param_init.input_buf[1].fmt_4cc = value;
	} else if (strcmp(name, "ovl1.buffer_type") == 0) {
		dev->param_init.input_buf[1].buf_type = value;
	} else if (strcmp(name, "ovl1.buffer_count") == 0) {
		dev->param_init.input_buf[1].buf_cnt = value;
	} else if (strcmp(name, "ovl1.plane_count") == 0) {
		dev->param_init.input_buf[1].plane_cnt = value;
	/*output_buf inputbuf setting */
	} else if (strcmp(name, "wdma.enable") == 0) {
		dev->param_init.output_buf.enable = value;
	} else if (strcmp(name, "wdma.width") == 0) {
		dev->param_init.output_buf.width = value;
	} else if (strcmp(name, "wdma.height") == 0) {
		dev->param_init.output_buf.height = value;
	} else if (strcmp(name, "wdma.pix_format") == 0) {
		dev->param_init.output_buf.fmt_4cc = value;
	} else if (strcmp(name, "wdma.buffer_type") == 0) {
		dev->param_init.output_buf.buf_type = value;
	} else if (strcmp(name, "wdma.buffer_count") == 0) {
		dev->param_init.output_buf.buf_cnt = value;
	} else if (strcmp(name, "wdma.plane_count") == 0) {
		dev->param_init.output_buf.plane_cnt = value;
	/*work param setting*/
	} else if (strcmp(name, "transition.area.width") == 0) {
		dev->param_work.transition_area.width = value;
	} else if (strcmp(name, "transition.area.height") == 0) {
		dev->param_work.transition_area.height = value;
	} else if (strcmp(name, "input0.area.width") == 0) {
		dev->param_work.input_area[0].width = value;
	} else if (strcmp(name, "input0.area.height") == 0) {
		dev->param_work.input_area[0].height = value;
	} else if (strcmp(name, "input1.area.width") == 0) {
		dev->param_work.input_area[1].width = value;
	} else if (strcmp(name, "input1.area.height") == 0) {
		dev->param_work.input_area[1].height = value;
	} else if (strcmp(name, "input0.coordinate.top") == 0) {
		dev->param_work.input_coordinate[0].top = value;
	} else if (strcmp(name, "input0.coordinate.left") == 0) {
		dev->param_work.input_coordinate[0].left = value;
	} else if (strcmp(name, "input1.coordinate.top") == 0) {
		dev->param_work.input_coordinate[1].top = value;
	} else if (strcmp(name, "input1.coordinate.left") == 0) {
		dev->param_work.input_coordinate[1].left = value;
	} else if (strcmp(name, "transition0.coordinate.top") == 0) {
		dev->param_work.transition_coordinate[0].top = value;
	} else if (strcmp(name, "transition0.coordinate.left") == 0) {
		dev->param_work.transition_coordinate[0].left = value;
	} else if (strcmp(name, "transition1.coordinate.top") == 0) {
		dev->param_work.transition_coordinate[1].top = value;
	} else if (strcmp(name, "transition1.coordinate.left") == 0) {
		dev->param_work.transition_coordinate[1].left = value;
	} else if (strcmp(name, "output.area.width") == 0) {
		dev->param_work.output_area.width = value;
	} else if (strcmp(name, "output.area.height") == 0) {
		dev->param_work.output_area.height = value;
	} else if (strcmp(name, "output.coordinate.top") == 0) {
		dev->param_work.output_coordinate.top = value;
	} else if (strcmp(name, "output.coordinate.left") == 0) {
		dev->param_work.output_coordinate.left = value;
	}
}

int ovl_create_buffer(struct ovl_device *dev)
{
	int dev_idx = 0;
	int buf_idx = 0;
	int ret;
	/* alloc buffer */

	for (dev_idx = 0; dev_idx < INPUT_DEV_COUNT; dev_idx++) {
		for (buf_idx = 0; buf_idx < dev->param_init.input_buf[dev_idx].buf_cnt; buf_idx++) {
			ret = mtk_ovl_test_alloc_buf(
				&(dev->input_buf_addr[dev_idx][buf_idx]),
				dev->param_init.input_buf[dev_idx].pitch * dev->param_init.input_buf[dev_idx].height,
				ALLOC_ALIGN);

			if (ret) {
				LOG_ERR("[E] fail to alloc input buf, dev_idx[%d], buf_idx[%d]", dev_idx, buf_idx);
				return -1;
			}
			dev->input_buf_addr[dev_idx][buf_idx].buf_id = buf_idx;

		}
	}
	/*
	for (buf_idx = 0; buf_idx < dev->param_init.output_buf.buf_cnt; buf_idx++) {
		ret = mtk_ovl_test_alloc_buf(
			&(dev->output_buf_addr[buf_idx]),
			dev->param_init.output_buf.pitch * dev->param_init.output_buf.height,
			ALLOC_ALIGN);

		if (ret) {
			LOG_ERR("[E] fail to alloc output buf_idx[%d]", buf_idx);
			return -1;
		}
		dev->output_buf_addr[buf_idx].buf_id = buf_idx;
	}
	*/
	for (buf_idx = 0; buf_idx < dev->param_init.output_buf.buf_cnt; buf_idx++) {
		fseek(dev->fd, 0, SEEK_SET);
		fread((void*)dev->input_buf_addr[1][buf_idx].va_align, 1, dev->length, dev->fd);
	}
	return 0;
}
int ovl_destroy_buffer(struct ovl_device *dev)
{
	int dev_idx = 0;
	int buf_idx = 0;
	int ret;
	/* free buffer */
	for (dev_idx = 0; dev_idx < INPUT_DEV_COUNT; dev_idx++) {
		for (buf_idx = 0; buf_idx < dev->param_init.input_buf[dev_idx].buf_cnt; buf_idx++) {
			ret = mtk_ovl_test_free_buf(&(dev->input_buf_addr[dev_idx][buf_idx]));
		}
	}
	/*
	for (buf_idx = 0; buf_idx < dev->param_init.output_buf.buf_cnt; buf_idx++) {
		ret = mtk_ovl_test_free_buf(&(dev->output_buf_addr[buf_idx]));
	}
	*/
	return 0;
}
static int ovl_driver_init(struct module *mod)
{
	int size;
	int pix_max;
	char header0[32];
	char header1[32];
	char header2[32];
	int		width;
	int		height;
	int 	i;
	struct ovl_device *dev;
	dev = (ovl_device*)module_get_driver_data(mod);
	MUTEX_INIT(dev->mutex_buffer);
	dev->fd = fopen(dev->guildeline_file, "rb");
	if (!dev->fd) {
		MOD_ERR(mod, "open fail: %s", dev->guildeline_file);
		return -1;
	}
	//sleep(10);
	v4l2_fix_planefmt(dev->param_init.input_buf[1].fmt_4cc, dev->param_init.input_buf[1].width,
					dev->param_init.input_buf[1].height, dev->plane_info, &dev->num_planes, 1);
	LOG_DBG("dev->num_planes is %d", dev->num_planes);
	dev->length = 0;
	for (i = 0; i < dev->num_planes; i++) {
		dev->length += dev->plane_info[i].sizeimage;
	}
	find_pitch(dev);
	//sleep(10);
	if( mtk_ovl_adapter_init(&(dev->handle),&(dev->param_init)) ){
		MOD_ERR(mod, "mtk_ovl_adapter_init fail");
		goto err;
	}
	ovl_create_buffer(dev);
	dump_buffer_info(dev);
	fclose(dev->fd);
	dev->fd = NULL;

	return 0;
err:
	fclose(dev->fd);
	dev->fd = NULL;
	return -1;
}

static void ovl_driver_deinit(struct module *mod)
{
	struct ovl_device *dev;
	dev = (ovl_device*)module_get_driver_data(mod);

	mtk_ovl_adapter_uninit(dev->handle);
    ovl_destroy_buffer(dev);

	if (dev->fd)
		fclose(dev->fd);
	dev->fd = NULL;
}
static int outfile_index = 0;
static int ovl_driver_handle_frame(struct module *mod)
{
	struct ovl_device *dev;
	struct shm_buff *in_frame, *out_frame;
	dev = (ovl_device*)module_get_driver_data(mod);
	int i = 0;
	int j = 0;
	void **va_list;
	int ret;
	char infile_path[256];
	char outfile_path[256];
	static char *record_buf[BUF_COUNT] = { NULL };
	static int buf_id[BUF_COUNT] = { 0 };
	/*
	* queue input frame
	*/
	in_frame = module_get_frame(mod);
    if (getRvcMode() == RvcMode_NormalRvc) {
        module_push_frame(mod, in_frame);
        LOG_DBG("LEAVE");
        return 0;
    }

	if (in_frame) {
		LOG_VERBOSE(1000, "get frame");
		if (module_if_ours_buffer(mod, in_frame)) {
			for (i = 0; i < BUF_COUNT; i++) {
				if ( &(dev->input_buf_addr[0][i]) == in_frame->buff) {
					break;
				}
			}
			if (i == BUF_COUNT) {
				MOD_ERR(mod, "can't find buffer, return");
				return -1;
			}

			dev->param_work.input_addr[0].addr_va = dev->input_buf_addr[0][i].va_align;
			dev->param_work.input_addr[0].index = dev->input_buf_addr[0][i].buf_id;
			dev->param_work.input_addr[1].addr_va = dev->input_buf_addr[1][i].va_align;
			dev->param_work.input_addr[1].index = dev->input_buf_addr[1][i].buf_id;
			out_frame = module_next_alloc_buffer(mod, dev->output_buf_addr[i].size);
			if(out_frame) {
				for (i=0; i<BUF_COUNT; i++) {
					if (record_buf[i] == shm_get_ptr(out_frame)) {
						dev->param_work.output_addr.addr_va = (unsigned long)shm_get_ptr(out_frame);
						//dev->param_work.output_addr.index = buf_id[i];
						dev->param_work.output_addr.index = i;
						break;
					}
				}
				if(i >= BUF_COUNT) {
					for(j=0; j<BUF_COUNT; j++) {
						if(record_buf[j] == NULL ){
							record_buf[j] = shm_get_ptr(out_frame);
							buf_id[j] = j;
							dev->param_work.output_addr.addr_va = (unsigned long)shm_get_ptr(out_frame);
							//dev->param_work.output_addr.index = buf_id[j];
							dev->param_work.output_addr.index = j;
							break;
						}
					}
				}
				ret = mtk_ovl_adapter_work(dev->handle, &(dev->param_work));
				if ( ret !=0 ) {
					LOG_WARN("mtk_ovl_adapter_work error");
				}

				module_push_frame(mod, out_frame);
			}

			shm_free(in_frame);
		} else {
			//don't support
		}
		LOG_DBG("LEAVE");
	}
	usleep(10);
	return 0;
}

module_driver ovl_driver = {
	.config = ovl_driver_config,
	.init = ovl_driver_init,
	.deinit = ovl_driver_deinit,
	.handle_frame = ovl_driver_handle_frame,
};

int ovl_module_init(struct module *mod)
{
	struct ovl_device *dev;
	dev = (ovl_device*)malloc(sizeof(ovl_device));
	memset(dev, 0, sizeof(*dev));

	module_set_name(mod, "ovl");
	module_register_driver(mod, &ovl_driver, dev);
	module_register_allocator(mod, &ovl_mem_ops, dev);
	return 0;
}
