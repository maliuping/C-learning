
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "videodev2.h"
#include <linux/v4l2-dv-timings.h>
#include "mtk_ovl_adapter.h"

static int g_adapter_log = 0;

#define LOG_TAG "MTK_OVL_ADAPTER"
#define MY_LOG(fmt, ...) \
	if (g_adapter_log) \
		printf("[%s] L[%d] "fmt"\n", LOG_TAG, __LINE__, ##__VA_ARGS__);

#define LOG_LINE  MY_LOG("%s[%d]", __func__, __LINE__)
#define LOG_START MY_LOG("%s start", __func__)
#define LOG_END   MY_LOG("%s end", __func__)

#define IOCTL(fd, cmd, p, ret) \
	MY_LOG("start to do ioc %s", #cmd); \
	*ret = ioctl(fd, cmd, p); \
	if (0 != *ret) {\
		MY_LOG("[E] ioc %s fail ret %d, errno is %d", #cmd, *ret, errno); \
	} else { \
		MY_LOG("ioc %s ok", #cmd); \
	}

#define INPUT_DEV_COUNT 2

#define VIDEO_DEV_NAME     "/dev/video"
#define INPUT_MODULE_NAME  "mtk-ovl-"
#define OUTPUT_MODULE_NAME "mtk-wdma"

#define MY_ALIGN(v, m) (((v) + (m) - 1) / (m) * (m))

struct MTK_OVL_ADAPTER_HANDLE {
	unsigned int input_fd[INPUT_DEV_COUNT];
	unsigned int output_fd;
};

struct MTK_OVL_ADAPTER_INSTANCE {
	struct MTK_OVL_ADAPTER_HANDLE dev_handle;
	struct MTK_OVL_ADAPTER_BUF_INFO input_buf[INPUT_DEV_COUNT];
	struct MTK_OVL_ADAPTER_BUF_INFO output_buf;
};

static int mtk_ovl_adapter_fmt2plane(
	unsigned int format_fourcc, unsigned int w, unsigned int h,
	struct v4l2_plane_pix_format *plane_fmt, unsigned int *nplanes)
{
	int align_w, align_h;

	LOG_START;

	switch (format_fourcc) {
	case V4L2_PIX_FMT_RGB565:
		*nplanes = 1;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		plane_fmt[0].bytesperline = align_w * 2;
		plane_fmt[0].sizeimage = align_w * 2 * align_h;
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
		*nplanes = 1;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		plane_fmt[0].bytesperline = align_w * 3;
		plane_fmt[0].sizeimage = align_w * 3 * align_h;
		break;
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
		*nplanes = 1;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		plane_fmt[0].bytesperline = align_w * 4;
		plane_fmt[0].sizeimage = align_w * 4 * align_h;
		break;
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
		*nplanes = 1;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		plane_fmt[0].bytesperline = align_w * 2;
		plane_fmt[0].sizeimage = align_w * 2 * align_h;
		break;
	case V4L2_PIX_FMT_NV12:
		/* YUV 420 scanline, 1 planes. */
		*nplanes = 1;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* CbCr plane */
		plane_fmt[1].bytesperline = align_w;
		plane_fmt[1].sizeimage = align_h * align_w / 2;
		break;
	case V4L2_PIX_FMT_NV12M:
		/* YUV 420 scanline, 2 planes. */
		*nplanes = 2;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* CbCr plane */
		plane_fmt[1].bytesperline = align_w;
		plane_fmt[1].sizeimage = align_h * align_w / 2;
		break;
	case V4L2_PIX_FMT_NV16:
		/* YUV 422 scanline, 1 planes. */
		*nplanes = 1;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* CbCr plane */
		plane_fmt[1].bytesperline = align_w;
		plane_fmt[1].sizeimage = align_h * align_w;
		break;
	case V4L2_PIX_FMT_NV16M:
		/* YUV 422 scanline, 2 planes. */
		*nplanes = 2;
		align_w = w; //(w + 15) & (~15);
		align_h = h; //(h + 15) & (~15);
		/* Y plane */
		plane_fmt[0].bytesperline = align_w;
		plane_fmt[0].sizeimage = align_w * align_h;
		/* CbCr plane */
		plane_fmt[1].bytesperline = align_w;
		plane_fmt[1].sizeimage = align_h * align_w;
		break;
	default:
		break;
	}

	MY_LOG("%s, fourcc[0x%x], wh[%d, %d], np[%d] [%d, %d, %d, %d]",
		__FUNCTION__,
		format_fourcc, w, h, *nplanes,
		plane_fmt[0].bytesperline, plane_fmt[0].sizeimage,
		plane_fmt[1].bytesperline, plane_fmt[1].sizeimage);

	return 0;
}

static int mtk_ovl_adapter_open_dev(struct MTK_OVL_ADAPTER_HANDLE *p_dev_handle)
{
	int fd;
	int dev_loop_idx = 0;
	int input_dev_id = 0;
	char str_dev_id[3];
	char str_dev_name[20];
	char dev_node[64];
	struct v4l2_capability cap;

	LOG_START;

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		memset(str_dev_name, 0, sizeof(str_dev_name));
		memcpy(str_dev_name, INPUT_MODULE_NAME, sizeof(INPUT_MODULE_NAME));
		sprintf(str_dev_id, "%d", input_dev_id);
		strcat(str_dev_name, str_dev_id);

		for (dev_loop_idx = 0; dev_loop_idx < 64; dev_loop_idx++) {
			sprintf(dev_node, "%s%d", VIDEO_DEV_NAME, dev_loop_idx);
			fd = open(dev_node, O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
			if (-1 == fd) {
				continue;
			}

			memset(&cap, 0, sizeof(cap));
			ioctl(fd, VIDIOC_QUERYCAP, &cap);
			MY_LOG("input[%d] loop[%d] get dev[%s] cap[%s] id[%d]\n",
				input_dev_id, dev_loop_idx, dev_node, cap.driver, fd);
			if (strcmp(cap.driver, str_dev_name) != 0) {
				MY_LOG("input[%d] loop[%d] close dev[%s] cap[%s] id[%d]\n",
					input_dev_id, dev_loop_idx, dev_node, cap.driver, fd);
				close(fd);
				continue;
			}

			MY_LOG("input[%d] loop[%d] accept dev[%s] cap[%s] id[%d]\n",
				input_dev_id, dev_loop_idx, dev_node, cap.driver, fd);
			p_dev_handle->input_fd[input_dev_id] = fd;
			break;
		}
	}

	for (dev_loop_idx = 0; dev_loop_idx < 64; dev_loop_idx++) {
		sprintf(dev_node, "%s%d", VIDEO_DEV_NAME, dev_loop_idx);
		fd = open(dev_node, O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
		if (-1 == fd) {
			continue;
		}

		memset(&cap, 0, sizeof(cap));
		ioctl(fd, VIDIOC_QUERYCAP, &cap);
		MY_LOG("output loop[%d] get dev[%s] cap[%s] id[%d]\n",
			dev_loop_idx, dev_node, cap.driver, fd);
		if (strcmp(cap.driver, OUTPUT_MODULE_NAME) != 0) {
			MY_LOG("output loop[%d] close dev[%s] cap[%s] id[%d]\n",
				dev_loop_idx, dev_node, cap.driver, fd);
			close(fd);
			continue;
		}

		MY_LOG("output loop[%d] accept dev[%s] cap[%s] id[%d]\n",
			dev_loop_idx, dev_node, cap.driver, fd);
		p_dev_handle->output_fd = fd;
		break;
	}

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		if (-1 != p_dev_handle->input_fd[input_dev_id]) {
			MY_LOG("ok to open fd %d", p_dev_handle->input_fd[input_dev_id])
		} else {
			MY_LOG("fail to open device")
			return -1;
		}
	}

	if (-1 != p_dev_handle->output_fd) {
		MY_LOG("ok to open fd %d", p_dev_handle->output_fd)
	} else {
		MY_LOG("fail to open device")
		return -1;
	}

	return 0;
}

static int mtk_ovl_adapter_close_dev(
	struct MTK_OVL_ADAPTER_HANDLE *p_dev_handle)
{
	int input_dev_id = 0;

	LOG_START;

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		if (-1 != p_dev_handle->input_fd[input_dev_id]) {
			close(p_dev_handle->input_fd[input_dev_id]);
			p_dev_handle->input_fd[input_dev_id] = -1;
			MY_LOG("ok to close input dev[%d]", input_dev_id)
		}
	}

	if (-1 != p_dev_handle->output_fd) {
		close(p_dev_handle->output_fd);
		p_dev_handle->output_fd = -1;
		MY_LOG("ok to close output dev")
	}

	return 0;
}

static int mtk_ovl_adapter_create_buf(
	unsigned int fd, struct MTK_OVL_ADAPTER_BUF_INFO *param_fmt)
{
	int ret;
	int plane_idx;
	unsigned int nplanes;
	struct v4l2_plane_pix_format plane_fmt[VIDEO_MAX_PLANES];
	struct v4l2_format format;
	struct v4l2_requestbuffers reqbufs;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buffer;

	LOG_START;

	mtk_ovl_adapter_fmt2plane(
		param_fmt->fmt_4cc,
		param_fmt->width,
		param_fmt->height,
		plane_fmt,
		&nplanes);
	param_fmt->plane_cnt = nplanes;
	param_fmt->bytesperline = plane_fmt[0].bytesperline;

	memset(&format, 0, sizeof(format));
	format.type = param_fmt->buf_type;
	format.fmt.pix_mp.width = param_fmt->width;
	format.fmt.pix_mp.height = param_fmt->height;
	format.fmt.pix_mp.pixelformat = param_fmt->fmt_4cc;
	format.fmt.pix_mp.num_planes = nplanes;
	for (plane_idx = 0; plane_idx < nplanes; ++plane_idx) {
		format.fmt.pix_mp.plane_fmt[plane_idx].sizeimage =
			plane_fmt[plane_idx].sizeimage; /*lq-check*/
		format.fmt.pix_mp.plane_fmt[plane_idx].bytesperline =
			plane_fmt[plane_idx].bytesperline; /*lq-check*/
	}
	MY_LOG("before VIDIOC_S_FMT");
	//sleep(1);
	IOCTL(fd, VIDIOC_S_FMT, &format, &ret);

	memset(&reqbufs, 0, sizeof(reqbufs));
	reqbufs.count = param_fmt->buf_cnt;
	reqbufs.type = param_fmt->buf_type;
	reqbufs.memory = param_fmt->mem_type;
	//MY_ALIGN_LOG("before VIDIOC_REQBUFS");
	//sleep(1);
	IOCTL(fd, VIDIOC_REQBUFS, &reqbufs, &ret);

	MY_LOG("Create buffers done");

	return 1;
}

static int mtk_ovl_adapter_destroy_buf(
	unsigned int fd, struct MTK_OVL_ADAPTER_BUF_INFO *param_fmt)
{
	int ret;
	struct v4l2_requestbuffers reqbufs;

	LOG_START;

	memset(&reqbufs, 0, sizeof(reqbufs));
	reqbufs.count = 0;
	reqbufs.type = param_fmt->buf_type;
	reqbufs.memory = param_fmt->mem_type;
	IOCTL(fd, VIDIOC_REQBUFS, &reqbufs, &ret);

	MY_LOG("Destroy input buffers done");

	return ret;
}

static int mtk_ovl_adapter_set_selection(
	struct MTK_OVL_ADAPTER_INSTANCE *p_inst,
	struct MTK_OVL_ADAPTER_PARAM_WORK *param)
{
	int ret;
	int input_dev_id = 0;
	struct v4l2_selection sel;

	LOG_START;

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {

		if (0 == p_inst->input_buf[input_dev_id].enable) {
			continue;
		}

		memset(&sel, 0, sizeof(sel));
		sel.target = V4L2_SEL_TGT_CROP;
		sel.r.left = param->input_coordinate[input_dev_id].left;
		sel.r.top = param->input_coordinate[input_dev_id].top;
		sel.r.width = param->input_area[input_dev_id].width;
		sel.r.height = param->input_area[input_dev_id].height;
		MY_LOG("input[%d] VIDIOC_S_SELECTION [0x%x, %d, %d, %d, %d]",
			input_dev_id,
			sel.target, sel.r.left, sel.r.top, sel.r.width, sel.r.height);
		IOCTL(p_inst->dev_handle.input_fd[input_dev_id],
			VIDIOC_S_SELECTION, &sel, &ret);
		if(ret != 0)
		{
        	return ret;
		}
		memset(&sel, 0, sizeof(sel));
		sel.target = V4L2_SEL_TGT_COMPOSE;
		sel.r.left = param->transition_coordinate[input_dev_id].left;
		sel.r.top = param->transition_coordinate[input_dev_id].top;
		sel.r.width = param->transition_area.width;
		sel.r.height = param->transition_area.height;
		MY_LOG("input[%d] VIDIOC_S_SELECTION [0x%x, %d, %d, %d, %d]",
			input_dev_id,
			sel.target, sel.r.left, sel.r.top, sel.r.width, sel.r.height);
		IOCTL(p_inst->dev_handle.input_fd[input_dev_id],
			VIDIOC_S_SELECTION, &sel, &ret);
		if(ret != 0)
		{
        	return ret;
		}
	}

	memset(&sel, 0, sizeof(sel));
	sel.target = V4L2_SEL_TGT_COMPOSE;
	sel.r.left = 0;
	sel.r.top = 0;
	sel.r.width = param->transition_area.width;
	sel.r.height = param->transition_area.height;
	MY_LOG("output VIDIOC_S_SELECTION [0x%x, %d, %d, %d, %d]",
		sel.target, sel.r.left, sel.r.top, sel.r.width, sel.r.height);
	IOCTL(p_inst->dev_handle.output_fd, VIDIOC_S_SELECTION, &sel, &ret);
	if(ret != 0)
	{
		return ret;
	}

	memset(&sel, 0, sizeof(sel));
	sel.target = V4L2_SEL_TGT_CROP;
	sel.r.left = param->output_coordinate.left;
	sel.r.top = param->output_coordinate.top;
	sel.r.width = param->output_area.width;
	sel.r.height = param->output_area.height;
	MY_LOG("output VIDIOC_S_SELECTION [0x%x, %d, %d, %d, %d]",
		sel.target, sel.r.left, sel.r.top, sel.r.width, sel.r.height);
	IOCTL(p_inst->dev_handle.output_fd, VIDIOC_S_SELECTION, &sel, &ret);
	if(ret != 0)
	{
		return ret;
	}

	return 0;
}

static int mtk_ovl_adapter_qbuf(
	struct MTK_OVL_ADAPTER_INSTANCE *p_inst,
	struct MTK_OVL_ADAPTER_PARAM_WORK *param)
{
	int ret;
	int input_dev_id = 0;
	struct v4l2_buffer qbuf;
	struct v4l2_plane qbuf_planes[VIDEO_MAX_PLANES];
	struct MTK_OVL_ADAPTER_BUF_INFO *p_buf_info = NULL;

	LOG_START;

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		p_buf_info = &(p_inst->input_buf[input_dev_id]);

		if (0 == p_buf_info->enable) {
			continue;
		}

		if (0 == p_buf_info->stream_on) {
			IOCTL(
				p_inst->dev_handle.input_fd[input_dev_id],
				VIDIOC_STREAMON,
				&(p_buf_info->buf_type), &ret);
			p_buf_info->stream_on = 1;
			MY_LOG("input[%d] stream on", input_dev_id);
			if(ret != 0)
			{
        		return ret;
			}
		}

		memset(&qbuf, 0, sizeof(qbuf));
		memset(qbuf_planes, 0, sizeof(qbuf_planes));
		qbuf_planes[0].bytesused =
			p_buf_info->bytesperline * p_buf_info->height;
		qbuf_planes[0].length =
			p_buf_info->pitch * p_buf_info->height;
		qbuf_planes[0].m.userptr = param->input_addr[input_dev_id].addr_va;
		qbuf.m.planes = qbuf_planes;
		qbuf.index = param->input_addr[input_dev_id].index;
		qbuf.type = p_buf_info->buf_type;
		qbuf.memory = p_buf_info->mem_type;
		qbuf.length = p_buf_info->plane_cnt;
		qbuf.timestamp.tv_sec = p_buf_info->timestamp.tv_sec;
		qbuf.timestamp.tv_usec = p_buf_info->timestamp.tv_usec;
		/*
		MY_LOG("    qbuf_planes: bytesused[%d] length[%d] userptr[0x%llx]",
			qbuf_planes[0].bytesused,
			qbuf_planes[0].length,
			qbuf_planes[0].m.userptr);
		MY_LOG("    qbuf: index[%d] type[%d] memory[%d] length[%d]",
			qbuf.index,
			qbuf.type,
			qbuf.memory,
			qbuf.length);
		MY_LOG("input[%d] VIDIOC_QBUF addr[0x%llx]",
			input_dev_id, qbuf_planes[0].m.userptr);
		*/
		IOCTL(
			p_inst->dev_handle.input_fd[input_dev_id],
			VIDIOC_QBUF, &qbuf, &ret);
		if(ret != 0)
		{
        	return ret;
		}
	}

	p_buf_info = &(p_inst->output_buf);

	if (0 == p_buf_info->stream_on) {
		IOCTL(
			p_inst->dev_handle.output_fd,
			VIDIOC_STREAMON, &(p_buf_info->buf_type), &ret);
		p_buf_info->stream_on = 1;
		MY_LOG("output stream on", input_dev_id);
		if(ret != 0)
		{
        	return ret;
		}
	}

	memset(&qbuf, 0, sizeof(qbuf));
	memset(qbuf_planes, 0, sizeof(qbuf_planes));
	qbuf_planes[0].bytesused =
		p_buf_info->bytesperline * p_buf_info->height;
	qbuf_planes[0].length =
		p_buf_info->pitch * p_buf_info->height;
	qbuf_planes[0].m.userptr = param->output_addr.addr_va;
	qbuf.m.planes = qbuf_planes;
	qbuf.index = param->output_addr.index;
	qbuf.type = p_buf_info->buf_type;
	qbuf.memory = p_buf_info->mem_type;
	qbuf.length = p_buf_info->plane_cnt;
	/*
	MY_LOG("    qbuf_planes: bytesused[%d] length[%d] userptr[0x%llx]",
		qbuf_planes[0].bytesused,
		qbuf_planes[0].length,
		qbuf_planes[0].m.userptr);
	MY_LOG("    qbuf: index[%d] type[%d] memory[%d] length[%d]",
		qbuf.index,
		qbuf.type,
		qbuf.memory,
		qbuf.length);
	MY_LOG("output VIDIOC_QBUF addr[0x%llx]", qbuf_planes[0].m.userptr);
	*/
	IOCTL(p_inst->dev_handle.output_fd, VIDIOC_QBUF, &qbuf, &ret);
	if(ret != 0)
	{
        return ret;
	}
	return 0;
}

static int mtk_ovl_adapter_dqbuf(struct MTK_OVL_ADAPTER_INSTANCE *p_inst)
{
	int ret;
	int input_dev_id;
	struct v4l2_buffer dqbuf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct MTK_OVL_ADAPTER_BUF_INFO *p_buf_info = NULL;

	LOG_START;

	/* deq input buf */
	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		p_buf_info = &(p_inst->input_buf[input_dev_id]);
		if (0 == p_buf_info->enable)
			continue;

		memset(&dqbuf, 0, sizeof(dqbuf));
		memset(&planes, 0, sizeof(planes));
		dqbuf.type = p_buf_info->buf_type;
		dqbuf.memory = p_buf_info->mem_type;
		dqbuf.m.planes = planes;
		dqbuf.length = p_buf_info->plane_cnt;
		IOCTL(
			p_inst->dev_handle.input_fd[input_dev_id],
			VIDIOC_DQBUF, &dqbuf, &ret);
		if(ret != 0)
		{
        	return ret;
		}

		/* input stream off */
		if (0 != p_buf_info->stream_on) {
			IOCTL(
				p_inst->dev_handle.input_fd[input_dev_id],
				VIDIOC_STREAMOFF, &(p_buf_info->buf_type), &ret);
			p_buf_info->stream_on = 0;
			MY_LOG("output stream off");
			if(ret != 0)
			{
	        	return ret;
			}
		}
	}

	/* deq output buf */
	p_buf_info = &(p_inst->output_buf);
	memset(&dqbuf, 0, sizeof(dqbuf));
	memset(&planes, 0, sizeof(planes));
	dqbuf.type = p_buf_info->buf_type;
	dqbuf.memory = p_buf_info->mem_type;
	dqbuf.m.planes = planes;
	dqbuf.length = p_buf_info->plane_cnt;
	IOCTL(p_inst->dev_handle.output_fd, VIDIOC_DQBUF, &dqbuf, &ret);
	if(ret != 0)
	{
		return ret;
	}

	/* output stream off */
	if (0 != p_inst->output_buf.stream_on) {
		IOCTL(
			p_inst->dev_handle.output_fd,
			VIDIOC_STREAMOFF, &(p_inst->output_buf.buf_type), &ret);
		p_inst->output_buf.stream_on = 0;
		MY_LOG("output stream off");
		if(ret != 0)
		{
        	return ret;
		}
	}

	return 0;
}

int mtk_ovl_adapter_init(
	void **p_handle, struct MTK_OVL_ADAPTER_PARAM_INIT *param)
{
	int ret = 0;
	int input_enable_status = 0;
	int input_dev_id = 0;
	struct MTK_OVL_ADAPTER_INSTANCE *p_inst = NULL;
	struct MTK_OVL_ADAPTER_BUF_INFO *p_buf_info = NULL;

	LOG_START;

	MY_LOG("get init param[0x%x] :", param);
	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		p_buf_info = &(param->input_buf[input_dev_id]);
		p_buf_info->mem_type = V4L2_MEMORY_USERPTR;
		MY_LOG("    input[%d] enable[%d] wh[%d, %d] fmt[0x%x] type[%d, %d] cnt[%d, %d] time[%d, %d]",
			input_dev_id,
			p_buf_info->enable,
			p_buf_info->width,
			p_buf_info->height,
			p_buf_info->fmt_4cc,
			p_buf_info->buf_type,
			p_buf_info->mem_type,
			p_buf_info->buf_cnt,
			p_buf_info->plane_cnt,
			p_buf_info->timestamp.tv_sec,
			p_buf_info->timestamp.tv_usec);
	}
	p_buf_info = &(param->output_buf);
	p_buf_info->mem_type = V4L2_MEMORY_USERPTR;
	MY_LOG("    output enable[%d] wh[%d, %d] fmt[0x%x] type[%d, %d] cnt[%d, %d] time[%d, %d]",
		p_buf_info->enable,
		p_buf_info->width,
		p_buf_info->height,
		p_buf_info->fmt_4cc,
		p_buf_info->buf_type,
		p_buf_info->mem_type,
		p_buf_info->buf_cnt,
		p_buf_info->plane_cnt,
		p_buf_info->timestamp.tv_sec,
		p_buf_info->timestamp.tv_usec);

	/* check param */
	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		if (param->input_buf[input_dev_id].enable) {
			input_enable_status = 1;
			break;
		}
	}
	if (0 == input_enable_status) {
		MY_LOG("input enable status error");
		return 1;
	}

	if (0 == param->output_buf.enable) {
		MY_LOG("output enable status error");
		return 1;
	}

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		p_buf_info = &(param->input_buf[input_dev_id]);
		if ((0 != p_buf_info->enable) &&
			(V4L2_MEMORY_USERPTR != p_buf_info->mem_type)) {
			MY_LOG("unsupport input mem_type %d",
				p_buf_info->mem_type);
			return 1;
		}
	}

	if (V4L2_MEMORY_USERPTR != param->output_buf.mem_type) {
		MY_LOG("unsupport output mem_type %d",
			param->output_buf.mem_type);
		return 1;
	}

	/* create instance */
	p_inst = malloc(sizeof(struct MTK_OVL_ADAPTER_INSTANCE));
	if (NULL == p_inst) {
		MY_LOG("fail to alloc inst, addr[0x%x], size[%d]",
			p_inst, (sizeof(struct MTK_OVL_ADAPTER_INSTANCE)));
		return 1;
	} else {
		MY_LOG("ok to alloc inst, addr[0x%x], size[%d]",
			p_inst, (sizeof(struct MTK_OVL_ADAPTER_INSTANCE)));
	}

	/* open device */
	ret = mtk_ovl_adapter_open_dev(&p_inst->dev_handle);
	if (ret) {
		MY_LOG("fail to open dev, ret = %d", ret);
		return ret;
	}

	/* create input buffer */
	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		p_buf_info = &(param->input_buf[input_dev_id]);
		if (param->input_buf[input_dev_id].enable) {
			p_buf_info->stream_on = 0;
			ret = mtk_ovl_adapter_create_buf(
				p_inst->dev_handle.input_fd[input_dev_id],
				&(param->input_buf[input_dev_id]));
		}
	}

	/* create output buffer */
	param->output_buf.stream_on = 0;
	mtk_ovl_adapter_create_buf(
		p_inst->dev_handle.output_fd, &param->output_buf);

	/* store param to instance */
	MY_LOG("[dbg] sizeof = [%d, %d]",
		sizeof(param->input_buf), sizeof(param->output_buf));
	memcpy(&p_inst->input_buf, &param->input_buf, sizeof(param->input_buf));
	memcpy(&p_inst->output_buf, &param->output_buf, sizeof(param->output_buf));

	/* exit */
	*p_handle = (void *)p_inst;

	return 0;
}

int mtk_ovl_adapter_uninit(void *handle)
{
	int ret = 0;
	int input_dev_id = 0;
	struct MTK_OVL_ADAPTER_BUF_INFO *p_buf_info = NULL;
	struct MTK_OVL_ADAPTER_INSTANCE *p_inst =
		(struct MTK_OVL_ADAPTER_INSTANCE *)handle;

	LOG_START;

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		p_buf_info = &(p_inst->input_buf[input_dev_id]);

		if (0 == p_buf_info->enable)
			continue;

		mtk_ovl_adapter_destroy_buf(
			p_inst->dev_handle.input_fd[input_dev_id],
			&p_inst->input_buf[input_dev_id]);
	}

	mtk_ovl_adapter_destroy_buf(
		p_inst->dev_handle.output_fd, &p_inst->output_buf);

	mtk_ovl_adapter_close_dev(&p_inst->dev_handle);

	free(handle);

	return 0;
}

int mtk_ovl_adapter_work(void *handle, struct MTK_OVL_ADAPTER_PARAM_WORK *param)
{
	int ret = 0;
	int idx = 0;
	int input_dev_id = 0;
	struct MTK_OVL_ADAPTER_INSTANCE *p_inst =
		(struct MTK_OVL_ADAPTER_INSTANCE *)handle;
	struct MTK_OVL_ADAPTER_BUF_ADDR *p_buf_addr = NULL;
	struct MTK_OVL_ADAPTER_BUF_INFO *p_buf_info = NULL;

	LOG_START;

	MY_LOG("get work param[0x%x] :", param);
	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		p_buf_addr = &(param->input_addr[input_dev_id]);
		MY_LOG("    input[%d] buf_addr[0x%llx] buf_idx[%d]",
			input_dev_id,
			p_buf_addr->addr_va,
			p_buf_addr->index);
	}
	p_buf_addr = &(param->output_addr);
	MY_LOG("    output buf_addr[0x%llx] buf_idx[%d]",
		p_buf_addr->addr_va,
		p_buf_addr->index);

	for (input_dev_id = 0; input_dev_id < INPUT_DEV_COUNT; input_dev_id++) {
		MY_LOG("    input[%d] area[%d, %d] coordinate[%d, %d]",
			input_dev_id,
			param->input_area[input_dev_id].width,
			param->input_area[input_dev_id].height,
			param->input_coordinate[input_dev_id].left,
			param->input_coordinate[input_dev_id].top);
		MY_LOG("    input[%d] transition_coordinate[%d, %d]",
			input_dev_id,
			param->transition_coordinate[input_dev_id].left,
			param->transition_coordinate[input_dev_id].top);
	}
	MY_LOG("    transition_area[%d, %d]",
		param->transition_area.width,
		param->transition_area.height);
	MY_LOG("    output area[%d, %d] coordinate[%d, %d]",
		param->output_area.width,
		param->output_area.height,
		param->output_coordinate.left,
		param->output_coordinate.top);

	ret = mtk_ovl_adapter_set_selection(p_inst, param);
	if(ret != 0)
	{
		printf("[%s] L[%d] set_selection fail\n", LOG_TAG, __LINE__);
        return ret;
	}
	ret = mtk_ovl_adapter_qbuf(p_inst, param);
	if(ret != 0)
	{
		printf("[%s] L[%d] qbuf fail\n", LOG_TAG, __LINE__);
        return ret;
	}

	ret = mtk_ovl_adapter_dqbuf(p_inst);
	if(ret != 0)
	{
		printf("[%s] L[%d] dqbuf fail\n", LOG_TAG, __LINE__);
        return ret;
	}

	MY_LOG("%s end", __func__);
	return ret;
}
int mtk_ovl_test_io(unsigned int is_read, char *path, void *addr, unsigned int req_size)
{
	int fd;
	int ret;
	unsigned int real_size;

	if (is_read) {
		fd = open(path, O_RDWR);
	} else {
		fd = open(path, O_RDWR | O_CREAT, S_IRWXU);
	}
	if (0 >= fd){
		MY_LOG("[E] fail to open %s, fd[%d]", path, fd);
	} else {
		MY_LOG("ok to open %s, fd[%d]", path, fd);
	}

	if (is_read) {
		real_size = read(fd, addr, req_size);
		if (real_size != req_size) {
			MY_LOG("[E] fail to read %s, fd[%d] addr[0x%llx] size[%d, %d]",
				path, fd, addr, req_size, real_size);
		} else {
			MY_LOG("ok to read %s, fd[%d] addr[0x%llx] size[%d, %d]",
				path, fd, addr, req_size, real_size);
		}
	} else {
		real_size = write(fd, addr, req_size);
		if (real_size != req_size) {
			MY_LOG("[E] fail to write %s, fd[%d] addr[0x%llx] size[%d, %d]",
				path, fd, addr, req_size, real_size);
		} else {
			MY_LOG("ok to write %s, fd[%d] addr[0x%llx] size[%d, %d]",
				path, fd, addr, req_size, real_size);
		}
	}

	ret = close(fd);
	if (0 != ret) {
		MY_LOG("[E] fail to close %s, fd[%d], ret[%d]", path, fd, ret);
	} else {
		MY_LOG("ok to close %s, fd[%d], ret[%d]", path, fd, ret);
	}
	return 0;
}

struct MTK_OVL_ADAPTER_API g_mtk_ovl_adapter_api = {
	mtk_ovl_adapter_init,
	mtk_ovl_adapter_uninit,
	mtk_ovl_adapter_work
};

