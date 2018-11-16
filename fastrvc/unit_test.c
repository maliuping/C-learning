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
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "videodev2.h"
#include "module.h"
#include "drm_display.h"
#include "util.h"
#include "xml.h"

static void sig_handler(int sig);
static void help_cmd();
static int camera_test(char *argv[]);
static int h264_codec_test(char *argv[]);
static int mdp_test(char *argv[]);
static int tvd_test(char *argv[]);
static int fmt_to(char *argv[]);
static int drm_test(char *argv[]);
static int mipicsi0_test(char *argv[]);
static int xml_test(char *argv[]);
static int di_test(char *argv[]);

struct unit_test_item {
	char 			*name;
	unsigned int	argc;
	int (*test_main)(char *argv[]);
	char			*cmdline;
};

static struct unit_test_item ut_items[] = {
	{"camera", 7, camera_test, "dev_path in_w in_h in_4cc out_w out_h out_4cc"},
	{"h264_codec", 8, h264_codec_test, "file in_w in_h in_4cc frame_rate out_w out_h out_4cc"},
	{"mdp", 7, mdp_test, "file in_w in_h in_4cc out_w out_h out_4cc"},
	{"tvd", 3, tvd_test, "in_w in_h in_4cc"},
	{"fmtto", 8, fmt_to, "in_file in_w in_h in_4cc out_file out_w out_h out_4cc"},
	{"drm", 6, drm_test, "file x y w h 4cc"},
	{"mipicsi0", 7, mipicsi0_test, "camera_index in_w in_h in_4cc out_w out_h out_4cc"},
	{"xml", 2, xml_test, "file.xml item"},
	{"di", 4, di_test, "file in_w in_h in_4cc"},
};

#define TOTAL_UT_COUNT (sizeof(ut_items) / sizeof(struct unit_test_item))
#define TEST_OUTPUT_DIR "/tmp/rvc_ut_output"

int main(int argc, char *argv[])
{
	int i, j;
	int ut_total;
	char *test_name;
	int ret, fail, pass;

	/* signal handler */
	struct sigaction act;
	act.sa_handler = sig_handler;
	sigaction(SIGINT, &act, NULL);

	if (argc == 1) {
		help_cmd();
		return 0;
	}

	ret = mkdir(TEST_OUTPUT_DIR, 0777);
	if (ret == 0) {
		//
	} else if (errno == EEXIST) {
		//char cmd[256];
		//sprintf(cmd, "rm -rf %s", TEST_OUTPUT_DIR);
		//system(cmd);
		//ret = mkdir(TEST_OUTPUT_DIR, 0777);
		//if (ret != 0) {
		//	printf("mkdir error(%d): %s\n", errno, strerror(errno));
		//	return ret;
		//}
	} else {
		printf("mkdir error(%d): %s\n", errno, strerror(errno));
		return ret;
	}

	printf("output file path: %s\n", TEST_OUTPUT_DIR);
	device_path_init();

	pass = 0;
	fail = 0;
	ut_total = TOTAL_UT_COUNT;
	for (i = 1; i < argc; i++) {
		test_name = argv[i];
		for (j = 0; j < ut_total; j++) {
			if (strcmp(test_name, ut_items[j].name) == 0) {
				if ((argc - i - 1) < ut_items[j].argc) {
					printf("cmdline WRONG\n");
					help_cmd();
					fail++;
					goto go_ret;
				}

				ret = ut_items[j].test_main(&argv[i+1]);
				if (ret != 0) {
					fail++;
					printf("[%s] test FAIL(%d)\n", test_name, ret);
				} else {
					pass++;
					printf("[%s] test PASS\n", test_name);
				}

				i += ut_items[j].argc;
				break;
			}
		}
		if (j >= ut_total) {
			fail++;
			printf("[%s] item UNSUPPORT\n", test_name);
			help_cmd();
		}
	}

go_ret:
	printf("test done, total pass(%d) fail(%d)\n", pass, fail);
	return -fail;
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
			exit(0);
			break;
	}
}

static void help_cmd()
{
	int i;
	int ut_total;

	printf("rvc_ut:\n");
	ut_total = TOTAL_UT_COUNT;
	for (i = 0; i < ut_total; i++) {
		printf("    %s", ut_items[i].name);
		if (ut_items[i].cmdline)
			printf(" %s", ut_items[i].cmdline);
		printf("\n");
	}
}

#define CODEC_FILE_DUMP 0
static int h264_codec_test(char *argv[])
{
	link_path *link;
	module *animated_logo;
	module *list2va;
	module *codec;
#if CODEC_FILE_DUMP
	char out_file[1024];
	module *fout;
#else
	module *mdp;
	module *drm;
#endif

	char *mdp_dev_path;
	char *h264dec_dev_path;
	char *in_file;
	unsigned int in_w, in_h, in_4cc, frame_rate;
	unsigned out_w, out_h, out_4cc, drm_fmt;

	mdp_dev_path = device_get_path("mtk-mdp");
	h264dec_dev_path = device_get_path("mtk-vcodec-dec");
	if (!mdp_dev_path || !h264dec_dev_path) {
		LOG_ERR("device path error");
		return -1;
	}

	in_file = argv[0];
	in_w = atoi(argv[1]);
	in_h = atoi(argv[2]);
	in_4cc = find_v4l2_type(argv[3]);
	frame_rate = atoi(argv[4]);
	out_w = atoi(argv[5]);
	out_h = atoi(argv[6]);
	out_4cc = find_v4l2_type(argv[7]);
	drm_fmt = find_drm_type(argv[7]);

	if (in_4cc == 0 || out_4cc == 0 || drm_fmt == 0) {
		printf("unsupport %s ==> %s\n", argv[3], argv[7]);
		return -1;
	}

	animated_logo = module_create(h264_logo_module_init);
	list2va = module_create(list2va_module_init);
	codec = module_create(v4l2_codec_module_init);
#if CODEC_FILE_DUMP
	fout = module_create(fout_module_init);
#else
	mdp = module_create(v4l2_mdp_module_init);
	drm = module_create(drm_module_init);
#endif

	link = link_modules(NULL, animated_logo);
	link = link_modules(link, codec);
#if CODEC_FILE_DUMP
	link = link_modules(link, list2va);
	link = link_modules(link, fout);
#else
	link = link_modules(link, mdp);
	link = link_modules(link, list2va);
	link = link_modules(link, drm);
#endif

	/* input configs */
	module_config(animated_logo, "name", (unsigned long)in_file);
	module_config(animated_logo, "frame_rate", frame_rate);

#if CODEC_FILE_DUMP
	/* memory convertion configs */
	module_config(list2va, "width", in_w);
	module_config(list2va, "height", in_h);
	module_config(list2va, "pix_format", in_4cc);
#else
	module_config(list2va, "width", out_w);
	module_config(list2va, "height", out_h);
	module_config(list2va, "pix_format", out_4cc);
#endif

	/* codec private configs */
	module_config(codec, "name", (unsigned long)h264dec_dev_path);
	module_config(codec, "out_buff.count", 5);
	module_config(codec, "out_buff.pix_field", V4L2_FIELD_NONE);
	module_config(codec, "out_buff.pix_format", V4L2_PIX_FMT_H264);
	module_config(codec, "out_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	module_config(codec, "out_buff.width", in_w);
	module_config(codec, "out_buff.height", in_h);
	module_config(codec, "cap_buff.count", 10);
	module_config(codec, "cap_buff.pix_field", V4L2_FIELD_NONE);
	module_config(codec, "cap_buff.pix_format", in_4cc);
	module_config(codec, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(codec, "cap_buff.width", in_w);
	module_config(codec, "cap_buff.height", in_h);

#if CODEC_FILE_DUMP
	/*file out configs*/
	sprintf(out_file, "%s/%s", TEST_OUTPUT_DIR, "h264_codec_out");
	module_config(fout, "name", (unsigned long)out_file);
	module_config(fout, "max_count", 5);
#else
	/* mdp private configs */
	module_config(mdp, "name", (unsigned long)mdp_dev_path);
	module_config(mdp, "out_buff.count", 3);
	module_config(mdp, "out_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "out_buff.pix_format", in_4cc);
	module_config(mdp, "out_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	module_config(mdp, "out_buff.width", in_w);
	module_config(mdp, "out_buff.height", in_h);
	module_config(mdp, "out_buff.rotation", 0);
	module_config(mdp, "out_buff.crop.c.width", in_w);
	module_config(mdp, "out_buff.crop.c.height", in_h);
	module_config(mdp, "cap_buff.count", 3);
	module_config(mdp, "cap_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "cap_buff.pix_format", out_4cc);
	module_config(mdp, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(mdp, "cap_buff.width", out_w);
	module_config(mdp, "cap_buff.height", out_h);
	module_config(mdp, "cap_buff.crop.c.width", out_w);
	module_config(mdp, "cap_buff.crop.c.height", out_h);

	/* drm configs */
	module_config(drm, "src_w", out_w);
	module_config(drm, "src_h", out_h);
	module_config(drm, "dst_w", out_w);
	module_config(drm, "dst_h", out_h);
	module_config(drm, "fourcc", drm_fmt);
	module_config(drm, "plane_number", 1);
#endif

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}

#define MDP_FILE_DUMP 0
static int mdp_test(char *argv[])
{
	link_path *link;
	module *fin;
	module *va2list;
	module *mdp;
	module *list2va;
#if MDP_FILE_DUMP
	char out_file[1024];
	module *fout;
#else
	module *drm;
#endif

	char *mdp_dev_path;
	char *infile;
	unsigned int in_w, in_h;
	unsigned int in_4cc;
	unsigned int out_w, out_h;
	unsigned int out_4cc;
	unsigned int drm_fmt;

	mdp_dev_path = device_get_path("mtk-mdp");
	if (!mdp_dev_path) {
		LOG_ERR("device path error");
		return -1;
	}

	/*
	*cmdline: in_file in_w in_h in_4cc out_w out_h out_4cc
	*/
	infile = argv[0];
	in_w = atoi(argv[1]);
	in_h = atoi(argv[2]);
	in_4cc = find_v4l2_type(argv[3]);
	out_w = atoi(argv[4]);
	out_h = atoi(argv[5]);
	out_4cc = find_v4l2_type(argv[6]);
	drm_fmt = find_drm_type(argv[6]);

	if (in_4cc == 0 || out_4cc == 0 || drm_fmt == 0) {
		printf("unsupport %s ==> %s\n", argv[3], argv[6]);
		return -1;
	}

	fin = module_create(fin_module_init);
	va2list = module_create(va2list_module_init);
	mdp = module_create(v4l2_mdp_module_init);
	list2va = module_create(list2va_module_init);
#if MDP_FILE_DUMP
	fout = module_create(fout_module_init);
#else
	drm = module_create(drm_module_init);
#endif

	link = link_modules(NULL, fin);
	link = link_modules(link, va2list);
	link = link_modules(link, mdp);
	link = link_modules(link, list2va);
#if MDP_FILE_DUMP
	link = link_modules(link, fout);
#else
	link = link_modules(link, drm);
#endif

	/* input configs */
	module_config(fin, "name", (unsigned long)infile);
	module_config(fin, "frame_rate", 30);

	/* memory convertion configs */
	module_config(va2list, "width", in_w);
	module_config(va2list, "height", in_h);
	module_config(va2list, "pix_format", in_4cc);
	module_config(list2va, "width", out_w);
	module_config(list2va, "height", out_h);
	module_config(list2va, "pix_format", out_4cc);

	/* mdp private configs */
	module_config(mdp, "name", (unsigned long)mdp_dev_path);
	module_config(mdp, "out_buff.count", 3);
	module_config(mdp, "out_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "out_buff.pix_format", in_4cc);
	module_config(mdp, "out_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	module_config(mdp, "out_buff.width", in_w);
	module_config(mdp, "out_buff.height", in_h);
	module_config(mdp, "out_buff.rotation", 0);
	module_config(mdp, "out_buff.crop.c.width", in_w);
	module_config(mdp, "out_buff.crop.c.height", in_h);
	module_config(mdp, "cap_buff.count", 3);
	module_config(mdp, "cap_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "cap_buff.pix_format", out_4cc);
	module_config(mdp, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(mdp, "cap_buff.width", out_w);
	module_config(mdp, "cap_buff.height", out_h);
	module_config(mdp, "cap_buff.crop.c.width", out_w);
	module_config(mdp, "cap_buff.crop.c.height", out_h);

#if MDP_FILE_DUMP
	/*file out configs*/
	sprintf(out_file, "%s/%s", TEST_OUTPUT_DIR, "mdp_out");
	module_config(fout, "name", (unsigned long)out_file);
	module_config(fout, "max_count", 5);
#else
	/* drm configs */
	module_config(drm, "src_w", out_w);
	module_config(drm, "src_h", out_h);
	module_config(drm, "dst_w", out_w);
	module_config(drm, "dst_h", out_h);
	module_config(drm, "fourcc", drm_fmt);
	module_config(drm, "plane_number", 1);
#endif

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}

static int tvd_test(char *argv[])
{
	char out_file[1024];
	link_path *link;
	module *tvd;
	module *list2va;
	module *fout;
	char *tvd_dev_path;

	tvd_dev_path = device_get_path("mtk-tvd");
	if (!tvd_dev_path) {
		LOG_ERR("device path error");
		return -1;
	}

	unsigned int in_w, in_h, in_4cc;
	in_w = atoi(argv[0]);
	in_h = atoi(argv[1]);
	in_4cc = find_v4l2_type(argv[2]);

	if (in_4cc == 0 ) {
		printf("unsupport %s\n", argv[2]);
		return -1;
	}

	tvd = module_create(v4l2_camera_module_init);
	list2va = module_create(list2va_module_init);
	fout = module_create(fout_module_init);

	link = link_modules(NULL, tvd);
	link = link_modules(link, list2va);
	link = link_modules(link, fout);

	/* memory convertion configs */
	module_config(list2va, "width", in_w);
	module_config(list2va, "height", in_h);
	module_config(list2va, "pix_format", in_4cc);

	/* camera configs */
	module_config(tvd, "name", (unsigned long)tvd_dev_path);
	module_config(tvd, "out_buff.count", 0);
	module_config(tvd, "cap_buff.count", 6);
	module_config(tvd, "cap_buff.width", in_w);
	module_config(tvd, "cap_buff.height", in_h);
	module_config(tvd, "cap_buff.pix_format", in_4cc);
	module_config(tvd, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(tvd, "cap_buff.pix_field", V4L2_FIELD_INTERLACED);

	/*file out configs*/
	sprintf(out_file, "%s/%s", TEST_OUTPUT_DIR, "tvd_out");
	module_config(fout, "name", (unsigned long)out_file);
	module_config(fout, "max_count", 5);

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}

static int fmt_to(char *argv[])
{
	link_path *link;
	module *fin;
	module *va2list;
	module *mdp;
	module *list2va;
	module *fout;

	char *mdp_dev_path;
	char *infile, *outfile;
	unsigned int in_w, in_h, out_w, out_h;
	unsigned int in_4cc, out_4cc;

	mdp_dev_path = device_get_path("mtk-mdp");
	if (!mdp_dev_path) {
		LOG_ERR("device path error");
		return -1;
	}

	/*
	* cmdline: in_file width height 4cc out_file width height 4cc
	*/
	infile = argv[0];
	in_w = atoi(argv[1]);
	in_h = atoi(argv[2]);
	in_4cc = find_v4l2_type(argv[3]);
	outfile = argv[4];
	out_w = atoi(argv[5]);
	out_h = atoi(argv[6]);
	out_4cc = find_v4l2_type(argv[7]);

	if (in_4cc == 0 || out_4cc == 0) {
		printf("unsupport %s ==> %s\n", argv[3], argv[7]);
		return -1;
	}

	fin = module_create(fin_module_init);
	va2list = module_create(va2list_module_init);
	mdp = module_create(v4l2_mdp_module_init);
	list2va = module_create(list2va_module_init);
	fout = module_create(fout_module_init);

	link = link_modules(NULL, fin);
	link = link_modules(link, va2list);
	link = link_modules(link, mdp);
	link = link_modules(link, list2va);
	link = link_modules(link, fout);

	/* input configs */
	module_config(fin, "name", (unsigned long)infile);
	module_config(fin, "frame_rate", 30);

	/* memory convertion configs */
	module_config(va2list, "width", in_w);
	module_config(va2list, "height", in_h);
	module_config(va2list, "pix_format", in_4cc);
	module_config(list2va, "width", out_w);
	module_config(list2va, "height", out_h);
	module_config(list2va, "pix_format", out_4cc);

	/* mdp private configs */
	module_config(mdp, "name", (unsigned long)mdp_dev_path);
	module_config(mdp, "out_buff.count", 3);
	module_config(mdp, "out_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "out_buff.pix_format", in_4cc);
	module_config(mdp, "out_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	module_config(mdp, "out_buff.width", in_w);
	module_config(mdp, "out_buff.height", in_h);
	module_config(mdp, "out_buff.rotation", 0);
	module_config(mdp, "out_buff.crop.c.width", in_w);
	module_config(mdp, "out_buff.crop.c.height", in_h);
	module_config(mdp, "cap_buff.count", 3);
	module_config(mdp, "cap_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "cap_buff.pix_format", out_4cc);
	module_config(mdp, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(mdp, "cap_buff.width", out_w);
	module_config(mdp, "cap_buff.height", out_h);
	module_config(mdp, "cap_buff.crop.c.width", out_w);
	module_config(mdp, "cap_buff.crop.c.height", out_h);

	/*file out configs*/
	module_config(fout, "name", (unsigned long)outfile);
	module_config(fout, "max_count", 1);

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(2);
	link_stop_modules(link);

	return 0;
}

static int drm_test(char *argv[])
{
	link_path *link;
	module *fin;
	module *drm;

	char *infile;
	unsigned int x, y;
	unsigned int w, h;
	unsigned int in_4cc;
	unsigned int drm_fmt;

	/*
	*cmdline: file x y w h 4cc
	*/
	infile = argv[0];
	x = atoi(argv[1]);
	y = atoi(argv[2]);
	w = atoi(argv[3]);
	h = atoi(argv[4]);
	in_4cc = find_v4l2_type(argv[5]);
	drm_fmt = find_drm_type(argv[5]);

	if (in_4cc == 0 || drm_fmt == 0) {
		printf("unsupport %s\n", argv[5]);
		return -1;
	}

	fin = module_create(fin_module_init);
	drm = module_create(drm_module_init);

	link = link_modules(NULL, fin);
	link = link_modules(link, drm);

	/* input configs */
	module_config(fin, "name", (unsigned long)infile);
	module_config(fin, "frame_rate", 30);

	/* drm configs */
	module_config(drm, "src_w", w);
	module_config(drm, "src_h", h);
	module_config(drm, "dst_x", x);
	module_config(drm, "dst_y", y);
	module_config(drm, "dst_w", w);
	module_config(drm, "dst_h", h);
	module_config(drm, "fourcc", drm_fmt);
	module_config(drm, "plane_number", 1);

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}

static int camera_test(char *argv[])
{
	link_path *link;
	module *camera;
	module *list2va;
	module *mdp;
	module *drm;

	char *mdp_dev_path;
	char *dev_path;
	unsigned int in_w, in_h, in_4cc, out_4cc, drm_fmt;
	unsigned int out_w, out_h;

	mdp_dev_path = device_get_path("mtk-mdp");
	if (!mdp_dev_path) {
		LOG_ERR("device path error");
		return -1;
	}

	dev_path = argv[0];
	in_w = atoi(argv[1]);
	in_h = atoi(argv[2]);
	in_4cc = find_v4l2_type(argv[3]);
	out_w = atoi(argv[4]);
	out_h = atoi(argv[5]);
	out_4cc = find_v4l2_type(argv[6]);
	drm_fmt = find_drm_type(argv[6]);

	if (in_4cc == 0 || out_4cc == 0 || drm_fmt == 0) {
		printf("unsupport %s ==> %s\n", argv[3], argv[6]);
		return -1;
	}

	camera = module_create(v4l2_camera_module_init);
	mdp = module_create(v4l2_mdp_module_init);
	list2va = module_create(list2va_module_init);
	drm = module_create(drm_module_init);

	link = link_modules(NULL, camera);
	link = link_modules(link, mdp);
	link = link_modules(link, list2va);
	link = link_modules(link, drm);

	/* memory convertion configs */
	module_config(list2va, "width", out_w);
	module_config(list2va, "height", out_h);
	module_config(list2va, "pix_format", out_4cc);

	/* camera configs */
	module_config(camera, "name", (unsigned long)dev_path);
	module_config(camera, "out_buff.count", 0);
	module_config(camera, "cap_buff.count", 6);
	module_config(camera, "cap_buff.width", in_w);
	module_config(camera, "cap_buff.height", in_h);
	module_config(camera, "cap_buff.pix_format", in_4cc);
	module_config(camera, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(camera, "cap_buff.pix_field", V4L2_FIELD_INTERLACED);

	/* mdp private configs */
	module_config(mdp, "name", (unsigned long)mdp_dev_path);
	module_config(mdp, "out_buff.count", 3);
	module_config(mdp, "out_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "out_buff.pix_format", in_4cc);
	module_config(mdp, "out_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	module_config(mdp, "out_buff.width", in_w);
	module_config(mdp, "out_buff.height", in_h);
	module_config(mdp, "out_buff.rotation", 0);
	module_config(mdp, "out_buff.crop.c.width", in_w);
	module_config(mdp, "out_buff.crop.c.height", in_h);
	module_config(mdp, "cap_buff.count", 3);
	module_config(mdp, "cap_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "cap_buff.pix_format", out_4cc);
	module_config(mdp, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(mdp, "cap_buff.width", out_w);
	module_config(mdp, "cap_buff.height", out_h);
	module_config(mdp, "cap_buff.crop.c.width", out_w);
	module_config(mdp, "cap_buff.crop.c.height", out_h);

	/* drm configs */
	module_config(drm, "src_w", out_w);
	module_config(drm, "src_h", out_h);
	module_config(drm, "dst_w", out_w);
	module_config(drm, "dst_h", out_h);
	module_config(drm, "fourcc", drm_fmt);
	module_config(drm, "plane_number", 1);

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}

#define MIPICSI0_FILE_DUMP 0
static int mipicsi0_test(char *argv[])
{
	link_path *link;
	module *mipicsi0;
#if MIPICSI0_FILE_DUMP
	char out_file[1024];
	module *fout;
#else
	module *mdp;
	module *drm;
	module *va2list;
	module *list2va;
#endif
	char *mipicsi0_dev_path;
	char *mdp_dev_path;

	mdp_dev_path = device_get_path("mtk-mdp");
	mipicsi0_dev_path = device_get_path("mipicsi0");
	if (!mdp_dev_path || !mipicsi0_dev_path) {
		LOG_ERR("device path error");
		return -1;
	}

	unsigned int camera_index;
	unsigned int in_w, in_h, in_4cc;
	unsigned int out_w, out_h, out_4cc, drm_fmt;

	camera_index = atoi(argv[0]);
	in_w = atoi(argv[1]);
	in_h = atoi(argv[2]);
	in_4cc = find_v4l2_type(argv[3]);
	out_w = atoi(argv[4]);
	out_h = atoi(argv[5]);
	out_4cc = find_v4l2_type(argv[6]);
	drm_fmt = find_drm_type(argv[6]);

	if ((in_4cc == 0) || (out_4cc == 0) || (drm_fmt == 0)) {
		printf("unsupport %s ==> %s\n", argv[3], argv[6]);
		return -1;
	}

	mipicsi0 = module_create(fourinone_camera_module_init);
#if MIPICSI0_FILE_DUMP
	fout = module_create(fout_module_init);
#else
	va2list = module_create(va2list_module_init);
	mdp = module_create(v4l2_mdp_module_init);
	list2va = module_create(list2va_module_init);
	drm = module_create(drm_module_init);
#endif

	link = link_modules(NULL, mipicsi0);
#if MIPICSI0_FILE_DUMP
	link = link_modules(link, fout);
#else
	link = link_modules(link, va2list);
	link = link_modules(link, mdp);
	link = link_modules(link, list2va);
	link = link_modules(link, drm);
#endif

	/* camera configs */
	module_config(mipicsi0, "name", (unsigned long)mipicsi0_dev_path);
	module_config(mipicsi0, "camera_index", camera_index);
	module_config(mipicsi0, "out_buff.count", 0);
	module_config(mipicsi0, "cap_buff.count", 3 * 4);
	module_config(mipicsi0, "cap_buff.width", in_w);
	module_config(mipicsi0, "cap_buff.height", in_h);
	module_config(mipicsi0, "cap_buff.pix_format", in_4cc);
	module_config(mipicsi0, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE);
	module_config(mipicsi0, "cap_buff.pix_field", V4L2_FIELD_INTERLACED);

#if MIPICSI0_FILE_DUMP
	/*file out configs*/
	sprintf(out_file, "%s/%s", TEST_OUTPUT_DIR, "mipicsi0_out");
	module_config(fout, "name", (unsigned long)out_file);
	module_config(fout, "max_count", 5);
#else
	/* memory convertion configs */
	module_config(va2list, "width", in_w);
	module_config(va2list, "height", in_h);
	module_config(va2list, "pix_format", in_4cc);

	/* mdp configs */
	module_config(mdp, "name", (unsigned long)mdp_dev_path);
	module_config(mdp, "out_buff.count", 3);
	module_config(mdp, "out_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "out_buff.pix_format", in_4cc);
	module_config(mdp, "out_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	module_config(mdp, "out_buff.width", in_w);
	module_config(mdp, "out_buff.height", in_h);
	module_config(mdp, "out_buff.rotation", 0);
	module_config(mdp, "cap_buff.count", 3);
	module_config(mdp, "cap_buff.pix_field", V4L2_FIELD_NONE);
	module_config(mdp, "cap_buff.pix_format", out_4cc);
	module_config(mdp, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(mdp, "cap_buff.width", out_w);
	module_config(mdp, "cap_buff.height", out_h);

	/* memory convertion configs */
	module_config(list2va, "width", out_w);
	module_config(list2va, "height", out_h);
	module_config(list2va, "pix_format", out_4cc);

	/* drm configs */
	module_config(drm, "src_w", out_w);
	module_config(drm, "src_h", out_h);
	module_config(drm, "dst_w", out_w);
	module_config(drm, "dst_h", out_h);
	module_config(drm, "fourcc", drm_fmt);
	module_config(drm, "plane_number", 1);
#endif

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}

static int xml_test(char *argv[])
{
	void *handle;
	link_path *link;

	handle = xml_open(argv[0]);
	if (!handle)
		return -1;

	link = xml_create_link(handle, argv[1]);
	xml_close(handle);

	if (!link)
		return -1;

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}

static int di_test(char *argv[])
{
	link_path *link;
	module *fin;
	module *va2list;
	module *di;
	module *list2va;
	char out_file[1024];
	module *fout;

	char *di_dev_path;
	char *infile;
	unsigned int in_w, in_h;
	unsigned int in_4cc;

	di_dev_path = device_get_path("mtk-di");
	if (!di_dev_path) {
		LOG_ERR("device path error");
		return -1;
	}

	/*
	*cmdline: in_file in_w in_h in_4cc out_w out_h out_4cc
	*/
	infile = argv[0];
	in_w = atoi(argv[1]);
	in_h = atoi(argv[2]);
	in_4cc = find_v4l2_type(argv[3]);

	if (in_4cc == 0) {
		printf("unsupport %s\n", argv[3]);
		return -1;
	}

	fin = module_create(fin_module_init);
	va2list = module_create(va2list_module_init);
	di = module_create(v4l2_module_init);
	list2va = module_create(list2va_module_init);
	fout = module_create(fout_module_init);

	link = link_modules(NULL, fin);
	link = link_modules(link, va2list);
	link = link_modules(link, di);
	link = link_modules(link, list2va);
	link = link_modules(link, fout);

	/* input configs */
	module_config(fin, "name", (unsigned long)infile);
	module_config(fin, "frame_rate", 30);

	/* memory convertion configs */
	module_config(va2list, "width", in_w);
	module_config(va2list, "height", in_h);
	module_config(va2list, "pix_format", in_4cc);
	module_config(list2va, "width", in_w);
	module_config(list2va, "height", in_h);
	module_config(list2va, "pix_format", in_4cc);

	/* di private configs */
	module_config(di, "name", (unsigned long)di_dev_path);
	module_config(di, "out_buff.count", 3);
	module_config(di, "out_buff.pix_field", V4L2_FIELD_INTERLACED_TB);
	module_config(di, "out_buff.pix_format", in_4cc);
	module_config(di, "out_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	module_config(di, "out_buff.width", in_w);
	module_config(di, "out_buff.height", in_h);
	module_config(di, "out_buff.rotation", 0);
	module_config(di, "out_buff.crop.c.width", in_w);
	module_config(di, "out_buff.crop.c.height", in_h);
	module_config(di, "cap_buff.count", 3);
	module_config(di, "cap_buff.pix_field", V4L2_FIELD_NONE);
	module_config(di, "cap_buff.pix_format", in_4cc);
	module_config(di, "cap_buff.buffer_type", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	module_config(di, "cap_buff.width", in_w);
	module_config(di, "cap_buff.height", in_h);
	module_config(di, "cap_buff.crop.c.width", in_w);
	module_config(di, "cap_buff.crop.c.height", in_h);

	/*file out configs*/
	sprintf(out_file, "%s/%s", TEST_OUTPUT_DIR, "di_out");
	module_config(fout, "name", (unsigned long)out_file);
	module_config(fout, "max_count", 5);

	if (0 != link_init_modules(link))
		return -1;
	if (0 != link_active_modules(link))
		return -1;
	sleep(1000);
	link_stop_modules(link);

	return 0;
}
