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
#include "osal.h"
#include "drm_display.h"

static struct mtk_plane_status plane_status[MTK_MAX_PLANE];

static const char * const connector_type_names[] = {
    [DRM_MODE_CONNECTOR_Unknown]     = "Unknown",
    [DRM_MODE_CONNECTOR_VGA]         = "VGA",
    [DRM_MODE_CONNECTOR_DVII]        = "DVI-I",
    [DRM_MODE_CONNECTOR_DVID]        = "DVI-D",
    [DRM_MODE_CONNECTOR_DVIA]        = "DVI-A",
    [DRM_MODE_CONNECTOR_Composite]   = "Composite",
    [DRM_MODE_CONNECTOR_SVIDEO]      = "SVIDEO",
    [DRM_MODE_CONNECTOR_LVDS]        = "LVDS",
    [DRM_MODE_CONNECTOR_Component]   = "Component",
    [DRM_MODE_CONNECTOR_9PinDIN]     = "DIN",
    [DRM_MODE_CONNECTOR_DisplayPort] = "DP",
    [DRM_MODE_CONNECTOR_HDMIA]       = "HDMI-A",
    [DRM_MODE_CONNECTOR_HDMIB]       = "HDMI-B",
    [DRM_MODE_CONNECTOR_TV]          = "TV",
    [DRM_MODE_CONNECTOR_eDP]         = "eDP",
#ifdef DRM_MODE_CONNECTOR_DSI
    [DRM_MODE_CONNECTOR_VIRTUAL]     = "Virtual",
    [DRM_MODE_CONNECTOR_DSI]         = "DSI",
#endif
};

static void
make_connector_name(struct mtk_display * disp, const drmModeConnector *con)
{
    const char *type_name = NULL;

    if (con->connector_type < sizeof connector_type_names / sizeof connector_type_names[0])
        type_name = connector_type_names[con->connector_type];

    if (!type_name)
        type_name = "UNNAMED";

    snprintf(disp->crtc_name, sizeof (disp->crtc_name),
        "%s-%d", type_name, con->connector_type_id);

    return;
}

static int drm_connector_get_mode(struct mtk_display * disp, drmModeConnector *connector)
{
    int i;
    int preferred_flag = 0;
    int configured_flag = 0;
    drmModeModeInfoPtr preferred = NULL;
    drmModeModeInfoPtr configured = NULL;
    drmModeModeInfoPtr best = NULL;
    drmModeModeInfoPtr crtc_mode = NULL;

    if (!connector || !connector->count_modes)
        return -1;

    make_connector_name(disp, connector);

    if (disp->rsl_width && disp->rsl_height)
        configured_flag = 1;
    else
        preferred_flag = 1;

    for (i = 0; i < connector->count_modes; i++) {
        if (disp->rsl_width == connector->modes[i].hdisplay &&
            disp->rsl_height == connector->modes[i].vdisplay &&
            NULL == configured)
            configured = &connector->modes[i];

        if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED)
            preferred = &connector->modes[i];

        best =  &connector->modes[i];
    }

    if ((configured_flag) && (NULL != configured))
        crtc_mode = configured;
    else if ((preferred_flag) && (NULL != preferred))
        crtc_mode = preferred;
    else
        crtc_mode = best;

    memcpy(&disp->crtc_mode, crtc_mode, sizeof (drmModeModeInfo));

    return 0;
}

int drm_set_mode(struct mtk_display * disp)
{
    int ret = 0;
    uint32_t blob_id = 0;
    drmModeAtomicReq *req;

    LOG_DBG("crtc_id %d con_id %d ", disp->crtc_id, disp->con_id);

    LOG_DBG("vdisplay %d, hdisplay %d ",
        disp->crtc_mode.vdisplay, disp->crtc_mode.hdisplay);

    req = drmModeAtomicAlloc();
    if (!req) {
        return -1;
    }

    ret = drmModeAtomicAddProperty(req, disp->con_id,
                           disp->con_prop_id[WDRM_CONNECTOR_CRTC_ID],
                           disp->crtc_id);
    if (ret < 0) {
        LOG_ERR("failed to set connector crtc id ");
        return -1;
    }

    if (disp->wb_con_id != 0) {
        ret = drmModeAtomicAddProperty(req, disp->wb_con_id,
                           disp->con_prop_id[WDRM_CONNECTOR_CRTC_ID],
                           disp->crtc_id);
    }
    if (ret < 0) {
        LOG_ERR("failed to set writeback connector crtc id ");
    }

    ret = drmModeCreatePropertyBlob(disp->fd,
                                    &disp->crtc_mode, sizeof(disp->crtc_mode),
                                      &blob_id);
    if (ret < 0) {
        LOG_ERR("failed to create property blob ");
        return -1;
    }

    ret = drmModeAtomicAddProperty(req, disp->crtc_id,
                           disp->crtc_prop_id[WDRM_CRTC_MODE_ID],
                           disp->crtc_id ? blob_id : 0);
    if (ret < 0) {
        LOG_ERR("failed to set crtc mode id");
        return -1;
    }

    ret = drmModeAtomicAddProperty(req, disp->crtc_id,
                           disp->crtc_prop_id[WDRM_CRTC_ACTIVE],
                           disp->crtc_id ? true : false);
    if (ret < 0) {
        LOG_ERR("failed to enable crtc ");
        return -1;
    }

    ret = drmModeAtomicCommit(disp->fd, req,
                            DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_ATOMIC_ALLOW_MODESET,
                            disp);
    if (ret < 0) {
        LOG_ERR("atomic commit failed %s", strerror(errno));
        return -1;
    }

    drmModeAtomicFree(req);

    return ret;
}

void drm_set_plane_status(struct mtk_display * disp, struct mtk_plane_status * status, int plane_num)
{
    memcpy(&plane_status[plane_num], status, sizeof(struct mtk_plane_status));
}

int drm_set_plane(struct mtk_display * disp, void *user_data,
        struct raw_texture *data_plane_0, struct raw_texture *data_plane_1,
        struct raw_texture *data_plane_2, struct raw_texture *data_plane_3)
{
    int ret = 0;
    int fb_id = 0;
    drmModeAtomicReq *req;
    uint32_t flags = DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT;
    int i = 0;
    struct raw_texture *raw_data_ptr[4];

    raw_data_ptr[0] = data_plane_0;
    raw_data_ptr[1] = data_plane_1;
    raw_data_ptr[2] = data_plane_2;
    raw_data_ptr[3] = data_plane_3;

    req = drmModeAtomicAlloc();
    if (!req) {
        LOG_ERR("Debug");
        return -1;
    }

    for(i = 0; i < MTK_MAX_PLANE; i++) {
        if(raw_data_ptr[i] == NULL) {
            continue;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                               disp->plane[i].prop_id[WDRM_PLANE_FB_ID],
                               raw_data_ptr[i]->fb_id);
        if (ret < 0) {
            LOG_ERR("failed to set plane framebuffer id ");
            return -1;
        }

        fb_id = raw_data_ptr[i]->fb_id;
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                               disp->plane[i].prop_id[WDRM_PLANE_CRTC_ID],
                               fb_id ? disp->crtc_id : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane crtc id ");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                               disp->plane[i].prop_id[WDRM_PLANE_SRC_X],
                               fb_id ? (plane_status[i].src_x << 16) : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane src x ");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_SRC_Y],
                           fb_id ? (plane_status[i].src_y << 16) : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane src y ");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_SRC_W],
                           fb_id ? (plane_status[i].src_w << 16) : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane src width ");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_SRC_H],
                           fb_id ? (plane_status[i].src_h << 16) : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane src height ");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_CRTC_X],
                           fb_id ? plane_status[i].dst_x : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane crtc x ");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_CRTC_Y],
                           fb_id ?  plane_status[i].dst_y : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane crtc y ");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_CRTC_W],
                           fb_id ?  plane_status[i].dst_w : 0);
        if (ret < 0) {
            LOG_ERR("failed to set plane crtc width");
            return -1;
        }
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_CRTC_H],
                           fb_id ?  plane_status[i].dst_h : 0);

        if (ret < 0) {
            LOG_ERR("failed to set plane crtc height");
            return -1;
        }
    }
// temp fix: 2712 drm ALPHA & COLORKEY not ready
/*
    for(i = 0; i < MTK_MAX_PLANE; i++) {
        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_ALPHA],
                           plane_status[i].alpha);
        if (ret < 0) {
            LOG_ERR("failed to set plane alpha");
            return -1;
        }

        ret = drmModeAtomicAddProperty(req, disp->plane[i].plane_id,
                           disp->plane[i].prop_id[WDRM_PLANE_COLORKEY],
                           plane_status[i].colorkey);
        if (ret < 0) {
            LOG_ERR("failed to set plane colorkey");
            return -1;
        }
    }
*/
    LOG_BOOT_PROFILE("display first frame");
    ret = drmModeAtomicCommit(disp->fd, req, flags, user_data);
    if (ret < 0) {
        LOG_ERR("atomic commit failed %s", strerror(errno));
        drmModeAtomicFree(req);
        return -1;
    }

    drmModeAtomicFree(req);
    return ret;
}

int drm_alloc_gem(int fd, int width, int height, int fourcc, struct raw_texture * raw_data)
{
    void *map = NULL;
    struct drm_mode_create_dumb create_arg;
    struct drm_mode_map_dumb map_arg;
    struct drm_prime_handle prime_arg;
    int i, ret;
    unsigned int alloc_size;

    memset(&create_arg, 0, sizeof(create_arg));
    memset(&map_arg, 0, sizeof(map_arg));
    memset(&prime_arg, 0, sizeof(prime_arg));

    raw_data->width = width;
    raw_data->height = height;
    raw_data->fourcc = fourcc;

    for(i = 0; i < MTK_MAX_PLANE; i ++ )
        raw_data->fds[i] = -1;

    switch (raw_data->fourcc) {
    case DRM_FORMAT_NV12:
        raw_data->bpp = 12;
        raw_data->plane_nums = 2;
        break;

    case DRM_FORMAT_NV16:
        raw_data->bpp = 16;
        raw_data->plane_nums = 2;
        break;

    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_UYVY:
        raw_data->bpp = 16;
        raw_data->plane_nums = 1;
        break;

    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        raw_data->bpp = 32;
        raw_data->plane_nums = 1;
        break;
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        raw_data->bpp = 24;
        raw_data->plane_nums = 1;
        break;
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
        raw_data->bpp = 12;
        raw_data->plane_nums = 3;
        break;

    default:
        fprintf(stderr, "unsupported format 0x%08x\n",  raw_data->fourcc);
        return -1;
    }

    if (raw_data->plane_nums == 3) {
        if (raw_data->bpp == 12) {
            raw_data->pitch[0] = DRM_ALIGN(raw_data->width, 16);
            raw_data->pitch[1] = raw_data->pitch[0] / 2;
            raw_data->pitch[2] = raw_data->pitch[0] / 2;
            raw_data->offset[0] = 0;
            raw_data->offset[1] = raw_data->pitch[0] * raw_data->height;
            raw_data->offset[2] = raw_data->offset[1] + raw_data->pitch[1] * raw_data->height / 2;
            alloc_size = raw_data->offset[2] + raw_data->pitch[2] * raw_data->height / 2;
        } else {
            fprintf(stderr,"debug: please add new format 0x%x\n", raw_data->fourcc);
            return -1;
        }
    } else if (raw_data->plane_nums == 2) {
        raw_data->pitch[0] = DRM_ALIGN(raw_data->width, 16);
        raw_data->offset[0] = 0;
        if (raw_data->bpp == 16) {
            raw_data->pitch[1] = raw_data->pitch[0];
            raw_data->offset[1] = raw_data->pitch[0] * raw_data->height;
            alloc_size = raw_data->offset[1] + raw_data->pitch[1] * raw_data->height;
            fprintf(stderr,"debug:  %s %d alloc_size = %d o/p [%d %d]\n",
                __FUNCTION__, __LINE__, alloc_size, raw_data->offset[1], raw_data->pitch[1]);
        }
        else if (raw_data->bpp == 12) {
            raw_data->pitch[1] = raw_data->pitch[0];
            raw_data->offset[1] = raw_data->pitch[0] * raw_data->height;
            alloc_size = raw_data->offset[1] + raw_data->pitch[1] * raw_data->height;
        } else {
            fprintf(stderr,"debug: please add new format 0x%x\n", raw_data->fourcc);
            return -1;
        }
    } else {
        raw_data->pitch[0] = DRM_ALIGN(raw_data->width * raw_data->bpp / 8, 16);
        raw_data->offset[0] = 0;
        alloc_size = raw_data->pitch[0] * raw_data->height;
    }

    create_arg.bpp = 8;
    create_arg.width = alloc_size;
    create_arg.height = 1;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
    if (ret) {
            LOG_ERR("error: drmIoctl %d DRM_IOCTL_MODE_CREATE_DUMB fail %d", fd, ret);
            return -1;
    }

    map_arg.handle = create_arg.handle;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg);
    if (ret) {
            LOG_ERR("error: drmIoctl DRM_IOCTL_MODE_MAP_DUMB fail %d", ret);
            return -1;
    }

    map = mmap(0, create_arg.size, PROT_WRITE|PROT_READ , MAP_SHARED, fd, map_arg.offset);
    if (map == MAP_FAILED) {
            LOG_ERR("error: mmap fail : 0x%p", map);
            return -1;
    }

    prime_arg.handle = create_arg.handle;
    prime_arg.flags = DRM_CLOEXEC;
    ret = drmIoctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_arg);
    if (ret || prime_arg.fd == -1) {
            LOG_ERR("error: drmIoctl DRM_IOCTL_PRIME_HANDLE_TO_FD fail %d fd=%d",
                    ret,prime_arg.fd);
            return -1;
    }

    for (i = 0; i < raw_data->plane_nums; i++) {
        raw_data->fds[i] = prime_arg.fd;
        raw_data->handle[i] = create_arg.handle;
    }
    raw_data->texbuf = map;
    raw_data->size = create_arg.size;

    return 0;
}

int drm_free_gem(int fd, struct raw_texture * raw_data)
{
    struct drm_mode_destroy_dumb arg;
    int ret = 0;

    if (raw_data->texbuf){
        munmap(raw_data->texbuf, raw_data->size);
        raw_data->texbuf = NULL;

        //close(raw_data->fds[0]);

        memset(&arg, 0, sizeof(arg));
        arg.handle = raw_data->handle[0];

        ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
        if (ret)
            LOG_ERR("failed to destroy dumb buffer: %s",
                strerror(errno));
    }

    return ret;
}


int drm_buffer_release(struct mtk_display * disp,     struct raw_texture *raw_data)
{
    int ret = 0;

    if (raw_data->fb_id){
        drmModeRmFB(disp->fd, raw_data->fb_id);
        raw_data->fb_id = 0;
    }

    return ret;
}

int drm_buffer_prepare(struct mtk_display * disp,    struct raw_texture *raw_data)
{
    int ret = 0;

    if (drmModeAddFB2(disp->fd, raw_data->width, raw_data->height,
                            raw_data->fourcc, raw_data->handle,
                            raw_data->pitch, raw_data->offset,
                            &raw_data->fb_id, 0)) {
        LOG_ERR("failed to add raw_data fb: %s", strerror(errno));
        return -1;
    }
    return ret;
}

int drm_connector_find_mode(drmModeConnector *connector, const char *mode_str,
        const unsigned int vrefresh, drmModeModeInfo *mode)
{
    int i;

    if (!connector || !connector->count_modes)
        return -1;

    for (i = 0; i < connector->count_modes; i++) {
        if (!strcmp(connector->modes[i].name, mode_str)) {
            if (vrefresh == 0) {
                *mode = connector->modes[i];
                return 0;
            } else if (connector->modes[i].vrefresh == vrefresh) {
                *mode = connector->modes[i];
                return 0;
            }
        }
    }

    return -1;
}

int drm_find_crtc_for_connector(struct mtk_display * disp,
            drmModeRes *resources, drmModeConnector *connector)
{
    drmModeEncoder *encoder;
    uint32_t possible_crtcs;
    int i, j;
    int crtc_idx;

    for (j = 0; j < connector->count_encoders; j++) {
        encoder = drmModeGetEncoder(disp->fd, connector->encoders[j]);
        if (encoder == NULL) {
            LOG_ERR("Failed to get encoder.");
            return -1;
        }
        possible_crtcs = encoder->possible_crtcs;
        drmModeFreeEncoder(encoder);

        crtc_idx = 0;
        for (i = 0; i < resources->count_crtcs; i++) {
            if (possible_crtcs & (1 << i)) {
                if (crtc_idx == disp->crtc_idx)
                    return resources->crtcs[i];
                crtc_idx++;
            }
        }
    }

    return -1;
}

int drm_resource_dump(struct mtk_display * disp)
{
    int i = 0;
    int j = 0;
    printf("drm resource dump start: \n");
    printf("crtc_id %u \n", disp->crtc_id);
    for (i = 0; i < WDRM_CRTC__COUNT; i ++)
        printf("crtc_prop_id[%d] %u \n", i, disp->crtc_prop_id[i]);

    printf("con_id %u \n", disp->con_id);
    for (i = 0; i < WDRM_CONNECTOR__COUNT; i ++)
        printf("con_prop_id[%d] %u \n", i, disp->con_prop_id[i]);

    for (j = 0; j < MTK_MAX_PLANE; j ++) {
        printf("plane %d plane_id %u \n", j, disp->plane[j].plane_id);
        for (i = 0; i < WDRM_PLANE__COUNT; i ++)
            printf("plane %d prop_id[%d] %u \n", j, i, disp->plane[j].prop_id[i]);
    }

    printf("crtc mode dump: \n");
    printf("clock %u \n", disp->crtc_mode.clock);
    printf("hdisplay %u, hsync_start %u, hsync_end %u, htotal %u, hskew %u \n",
        disp->crtc_mode.hdisplay, disp->crtc_mode.hsync_start, disp->crtc_mode.hsync_end,
        disp->crtc_mode.htotal, disp->crtc_mode.hskew);
    printf("vdisplay %u, vsync_start %u, vsync_end %u, vtotal %u, vscan %u \n",
        disp->crtc_mode.vdisplay, disp->crtc_mode.vsync_start, disp->crtc_mode.vsync_end,
        disp->crtc_mode.vtotal, disp->crtc_mode.vscan);

    printf("vrefresh %u flags 0x%x type 0x%x name %s\n",
        disp->crtc_mode.vrefresh, disp->crtc_mode.flags, disp->crtc_mode.type, disp->crtc_mode.name);

    printf("drm resource dump done\n");
    return 0;
}

static void drm_get_fixed_resource_1(struct mtk_display * disp)
{
    int i = 0;

    disp->crtc_id = 30;
    disp->crtc_prop_id[WDRM_CRTC_MODE_ID] = 16;
    disp->crtc_prop_id[WDRM_CRTC_ACTIVE] = 15;
    disp->crtc_prop_id[WDRM_CRTC_BACKGROUND] = 21;

    disp->con_id = 23;
    disp->con_prop_id[WDRM_CONNECTOR_CRTC_ID] = 14;

    for(i = 0; i < MTK_MAX_PLANE; i++)
    {
        disp->plane[i].plane_id = 26 + i;
        disp->plane[i].prop_id[WDRM_PLANE_TYPE] = 4;
        disp->plane[i].prop_id[WDRM_PLANE_SRC_X] = 5;
        disp->plane[i].prop_id[WDRM_PLANE_SRC_Y] = 6;
        disp->plane[i].prop_id[WDRM_PLANE_SRC_W] = 7;
        disp->plane[i].prop_id[WDRM_PLANE_SRC_H] = 8;
        disp->plane[i].prop_id[WDRM_PLANE_CRTC_X] = 9;
        disp->plane[i].prop_id[WDRM_PLANE_CRTC_Y] = 10;
        disp->plane[i].prop_id[WDRM_PLANE_CRTC_W] = 11;
        disp->plane[i].prop_id[WDRM_PLANE_CRTC_H] = 12;
        disp->plane[i].prop_id[WDRM_PLANE_FB_ID] = 13;
        disp->plane[i].prop_id[WDRM_PLANE_CRTC_ID] = 14;
        disp->plane[i].prop_id[WDRM_PLANE_ALPHA] = 18;
        disp->plane[i].prop_id[WDRM_PLANE_COLORKEY] = 19;
    }

    disp->crtc_mode.clock = 142858;
    disp->crtc_mode.hdisplay = 1080;
    disp->crtc_mode.hsync_start = 1120;
    disp->crtc_mode.hsync_end = 1130;
    disp->crtc_mode.htotal = 1150;
    disp->crtc_mode.hskew = 0;

    disp->crtc_mode.vdisplay = 1920;
    disp->crtc_mode.vsync_start = 1930;
    disp->crtc_mode.vsync_end = 1932;
    disp->crtc_mode.vtotal = 1940;
    disp->crtc_mode.vscan = 0;

    disp->crtc_mode.vrefresh = 60;
    disp->crtc_mode.flags = 0x0;
    disp->crtc_mode.type = 0x0;
    strcpy(disp->crtc_mode.name, "1080x1920");
}

static void drm_get_fixed_resource_2(struct mtk_display * disp)
{

}

static int drm_get_resource(struct mtk_display * disp)
{
    drmModeConnector *connector;
    drmModeRes *resources;
    drmModePropertyPtr property;
    drmModePlaneRes *plane_res;
    drmModePlane *plane;
    drmModeObjectProperties *props;
    int plane_num = 0;
    int i, j, k;
    int crtc_idx, wb_crtc_idx;

    resources = drmModeGetResources(disp->fd);
    if (!resources) {
        LOG_ERR("drmModeGetResources failed");
        return -1;
    }

    crtc_idx = 0;
    wb_crtc_idx = 0;
    for (i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(disp->fd,
                        resources->connectors[i]);
        if (connector == NULL)
            continue;

        if ((connector->connection == DRM_MODE_CONNECTED) && (disp->con_id == 0)) {
            if (crtc_idx == disp->crtc_idx) {
                disp->crtc_id = drm_find_crtc_for_connector(disp, resources, connector);
                if (disp->crtc_id < 0) {
                    LOG_ERR("No usable crtc/encoder pair for connector.");
                    return -1;
                }

                if (drm_connector_get_mode(disp, connector) < 0){
                    LOG_ERR("Connector get mode error");
                    return -1;
                }

                disp->con_id = connector->connector_id;
                for (j = 0; j < connector->count_props; j++) {
                    property = drmModeGetProperty(disp->fd, connector->props[j]);
                    if (!property)
                        continue;
                    for(k = 0; k < WDRM_CONNECTOR__COUNT; k ++) {
                        if (0 == strcmp(property->name, mtk_connector_prop_names[k])) {
                            disp->con_prop_id[k] = property->prop_id;
                            LOG_DBG("connector prop name %s prop id %d",
                                    property->name, property->prop_id);
                            break;
                        }
                    }
                    drmModeFreeProperty(property);
                }
            }
            crtc_idx++;
        } else if ((connector->connection == DRM_MODE_DISCONNECTED) && (disp->wb_con_id == 0)) {
            if (wb_crtc_idx == disp->crtc_idx)
                disp->wb_con_id = connector->connector_id;
            wb_crtc_idx++;
        }

        drmModeFreeConnector(connector);

        if (disp->con_id && disp->wb_con_id)
            break;
    }

    if(disp->crtc_id != -1) {
        props = drmModeObjectGetProperties(disp->fd,
                        disp->crtc_id, DRM_MODE_OBJECT_CRTC);
        for (i = 0; i < props->count_props; i++) {
            property = drmModeGetProperty(disp->fd, props->props[i]);
            if (!property)
                continue;
            for(k = 0; k < WDRM_CRTC__COUNT; k ++) {
                if (0 == strcmp(property->name, mtk_crtc_prop_names[k])) {
                    disp->crtc_prop_id[k] = property->prop_id;
                    LOG_DBG("crtc %d prop name %s prop id %d",
                            disp->crtc_id, property->name, property->prop_id);
                    break;
                }
            }
            drmModeFreeProperty(property);
        }
        drmModeFreeObjectProperties(props);
    }

    drmModeFreeResources(resources);

    plane_res = drmModeGetPlaneResources(disp->fd);
    if (!plane_res) {
        LOG_ERR("failed to get plane resources: %s",
            strerror(errno));
        return -1;
    }

    LOG_DBG("plane num %d ", plane_res->count_planes);

    for (i = 0; i < plane_res->count_planes; i++) {
        plane = drmModeGetPlane(disp->fd, plane_res->planes[i]);
        if (!plane)
            continue;

        if((plane->possible_crtcs & (1 << disp->crtc_idx)) == 0)
            continue;

        LOG_DBG("creating new plane %d", plane_res->planes[i]);

        disp->plane[plane_num].plane_id = plane->plane_id;

        props = drmModeObjectGetProperties(disp->fd,
                        plane->plane_id, DRM_MODE_OBJECT_PLANE);
        for (j = 0; j < props->count_props; j++) {
            property = drmModeGetProperty(disp->fd, props->props[j]);
            if (!property)
                continue;
            for(k = 0; k < WDRM_PLANE__COUNT; k ++) {
                if (0 == strcmp(property->name, mtk_plane_prop_names[k])) {
                    disp->plane[plane_num].prop_id[k] = property->prop_id;
                    LOG_DBG("plane %d prop name %s prop id %d",
                            plane->plane_id, property->name, property->prop_id);
                    break;
                }
            }
            drmModeFreeProperty(property);
        }
        drmModeFreeObjectProperties(props);
        drmModeFreePlane(plane);
        plane_num ++;
    }

    drmModeFreePlaneResources(plane_res);

    return 0;
}

int drm_init(struct mtk_display * disp)
{
    int ret = 0;
    int i;
    char dev_name[64];
    struct stat st;

    //disp->fd = drmOpen(MTK_DRM_DRV_NAME, NULL);
    for (i = 0; i < 8; i++) {
        sprintf(dev_name, "/dev/dri/card%d", i);
        if (stat(dev_name, &st) == 0)
            break;
    }

    if (i >= 8) {
        LOG_ERR("failed to find drm device.");
        return -1;
    }

    // max wait 1s
    for (i = 0; i < 1000; i++) {
        disp->fd = open(dev_name, O_RDWR | O_CLOEXEC);
        if (disp->fd > 0)
            break;
        usleep(1000);
    }

    if (disp->fd < 0) {
        LOG_ERR("failed to open device.");
        return -1;
    }
    LOG_DBG("Debug disp->fd = %d", disp->fd);
    ret = drmDropMaster(disp->fd);
    if (ret) {
        LOG_WARN("drmDropMaster fail, be careful");
    }
    ret = drmSetClientCap(disp->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
        LOG_ERR("driver doesn't support universal planes setting");
        return -1;
    }

    ret = drmSetClientCap(disp->fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret) {
        LOG_ERR("driver doesn't support atomic display setting");
        return -1;
    }
    if(1 == disp->fix_resource)
        drm_get_fixed_resource_1(disp);
    else if (2 == disp->fix_resource)
        drm_get_fixed_resource_2(disp);
    else {
        ret = drm_get_resource(disp);
    }

    memset(plane_status, 0, MTK_MAX_PLANE * sizeof(struct mtk_plane_status));

    return ret;
}

int drm_deinit(struct mtk_display * disp)
{
    drmClose(disp->fd);
    return 0;
}
