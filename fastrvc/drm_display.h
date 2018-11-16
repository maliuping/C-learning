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

#ifndef _DRM_DISPLAY_H
#define _DRM_DISPLAY_H
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

//#include "fastrvc_util.h"

#define MTK_DRM_DRV_NAME "mediatek"

#define MTK_MAX_PLANE                4

#define DRM_ALIGN(X,bit) ((X + bit-1) & (~(bit-1)))

/**
 * List of properties attached to DRM connectors
 */
enum wdrm_connector_property {
    WDRM_CONNECTOR_CRTC_ID = 0,
    WDRM_CONNECTOR__COUNT
};

/**
 * List of properties attached to DRM CRTCs
 */
enum wdrm_crtc_property {
    WDRM_CRTC_MODE_ID = 0,
    WDRM_CRTC_ACTIVE,
    WDRM_CRTC_BACKGROUND,
    WDRM_CRTC__COUNT
};

/**
 * List of properties attached to DRM PLANEs
 */
enum wdrm_plane_property {
    WDRM_PLANE_TYPE = 0,
    WDRM_PLANE_SRC_X,
    WDRM_PLANE_SRC_Y,
    WDRM_PLANE_SRC_W,
    WDRM_PLANE_SRC_H,
    WDRM_PLANE_CRTC_X,
    WDRM_PLANE_CRTC_Y,
    WDRM_PLANE_CRTC_W,
    WDRM_PLANE_CRTC_H,
    WDRM_PLANE_FB_ID,
    WDRM_PLANE_CRTC_ID,
    WDRM_PLANE_ALPHA,
    WDRM_PLANE_COLORKEY,
    WDRM_PLANE__COUNT
};

struct raw_texture {
    /* input */
    int width;
    int height;
    int fourcc;
    int bpp;
    int plane_nums;

    unsigned int pitch[MTK_MAX_PLANE];
    unsigned int offset[MTK_MAX_PLANE];
    int fds[MTK_MAX_PLANE];
    unsigned int handle[MTK_MAX_PLANE];

    void *texbuf;
    int size;

    unsigned int fb_id;
    int  index;
};

struct mtk_plane_status {
    uint32_t src_x;
    uint32_t src_y;
    uint32_t src_w;
    uint32_t src_h;
    uint32_t dst_x;
    uint32_t dst_y;
    uint32_t dst_w;
    uint32_t dst_h;

    uint32_t alpha; /* bit[8]: enable=1, disable=0.  bit[7:0]:  alpha value 0~0xff*/
    uint32_t colorkey;/* Only support ARGB8888/RGB565/RGB888/BGR888.
                For ARGB8888: bit[31:25]: A . bit[24:16]: R . bit[15:8]: G . bit[7:0]: B
                For RGB565:  bit[31:25]: 0xff. bit[24:16]: 0 . bit[15:11]: R. bit[10:5]: G . bit[4:0]: B
                For RGB888/BGR888:  bit[31:25]: 0xff.  bit[24:16]: R . bit[15:8]: G . bit[7:0]: B*/
};

struct mtk_plane {
    uint32_t plane_id;
    uint32_t prop_id[WDRM_PLANE__COUNT];
};

struct mtk_display {
    int fd;

    int32_t rsl_width, rsl_height;

    uint32_t crtc_id;
    uint32_t crtc_prop_id[WDRM_CRTC__COUNT];

    uint32_t con_id;
    uint32_t wb_con_id;
    uint32_t con_prop_id[WDRM_CONNECTOR__COUNT];

    struct mtk_plane plane[MTK_MAX_PLANE];

    drmModeModeInfo crtc_mode;
    uint32_t blob_id;

    //struct raw_texture *raw_data;
    //int raw_tex_cnt;

    uint32_t swap_count;
    uint32_t capture_buf_id;
    struct timeval start;

    char crtc_name[32];
    int fix_resource;
    int crtc_idx;
};

static const char * const mtk_crtc_prop_names[] = {
    [WDRM_CRTC_MODE_ID] = "MODE_ID",
    [WDRM_CRTC_ACTIVE] = "ACTIVE",
    [WDRM_CRTC_BACKGROUND] = "background",
};
static const char * const mtk_connector_prop_names[] = {
    [WDRM_CONNECTOR_CRTC_ID] = "CRTC_ID",
};

static const char * const mtk_plane_prop_names[] = {
    [WDRM_PLANE_TYPE] = "type",
    [WDRM_PLANE_SRC_X] = "SRC_X",
    [WDRM_PLANE_SRC_Y] = "SRC_Y",
    [WDRM_PLANE_SRC_W] = "SRC_W",
    [WDRM_PLANE_SRC_H] = "SRC_H",
    [WDRM_PLANE_CRTC_X] = "CRTC_X",
    [WDRM_PLANE_CRTC_Y] = "CRTC_Y",
    [WDRM_PLANE_CRTC_W] = "CRTC_W",
    [WDRM_PLANE_CRTC_H] = "CRTC_H",
    [WDRM_PLANE_FB_ID] = "FB_ID",
    [WDRM_PLANE_CRTC_ID] = "CRTC_ID",
    [WDRM_PLANE_ALPHA] = "alpha",
    [WDRM_PLANE_COLORKEY] = "colorkey",
};

/* Function Statement */
int drm_set_mode(struct mtk_display * disp);

void drm_set_plane_status(struct mtk_display * disp, struct mtk_plane_status * status, int plane_num);

int drm_set_plane(struct mtk_display * disp, void *user_data,
        struct raw_texture *data_plane_0, struct raw_texture *data_plane_1,
        struct raw_texture *data_plane_2, struct raw_texture *data_plane_3);

int drm_alloc_gem(int fd, int width, int height, int fourcc, struct raw_texture * raw_data);

int drm_free_gem(int fd, struct raw_texture * raw_data);

int drm_buffer_release(struct mtk_display * disp,     struct raw_texture *raw_data);

int drm_buffer_prepare(struct mtk_display * disp,    struct raw_texture *raw_data);

int drm_connector_find_mode(drmModeConnector *connector, const char *mode_str,
        const unsigned int vrefresh, drmModeModeInfo *mode);

int drm_find_crtc_for_connector(struct mtk_display * disp,
            drmModeRes *resources, drmModeConnector *connector);

int drm_resource_dump(struct mtk_display * disp);

int drm_init(struct mtk_display * disp);

int drm_deinit(struct mtk_display * disp);

#endif
