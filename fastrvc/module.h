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
#ifndef FASTRVC_MODULE_H
#define FASTRVC_MODULE_H

#include "osal.h"
#include "shm_buff.h"
#include "queue.h"

#define MOD_CRIT(mod, fmt, args...)\
	LOG_CRIT("[mod:%s] " fmt, mod->name, ##args)
#define MOD_ERR(mod, fmt, args...)\
	LOG_ERR("[mod:%s] " fmt, mod->name, ##args)
#define MOD_WARN(mod, fmt, args...)\
	LOG_WARN("[mod:%s] " fmt, mod->name, ##args)
#define MOD_INFO(mod, fmt, args...)\
	LOG_INFO("[mod:%s] " fmt, mod->name, ##args)
#define MOD_DBG(mod, fmt, args...)\
	LOG_DBG("[mod:%s] " fmt, mod->name, ##args)

/* module related */

typedef struct module {
	struct module *prev, *next;
	char *name;
	int ref_count;

	int status;
	MUTEX_HANDLE(mutex_status);
	COND_HANDLE(cond_status);

	THREAD_HANDLE(thread_main);
	struct queue *q_media;

	struct module_driver *driver;
	void *driver_data;

	struct memory_allocator *allocator;

	int in_mem_type;
	int out_mem_type;

#ifdef PERFORMANCE_MONITOR
	struct timeval time_start;
	unsigned int frame_count;
	unsigned int fps_update_count;
	double output_fps;
	double input_fps;
#endif
}module;

typedef struct module_driver {
	void (*config)(module *, char *, unsigned long);
	int (*init)(module *);
	void (*deinit)(module *);
	int (*handle_frame)(module *);
	int (*pause)(module *);
} module_driver;

/* flags for 'status' */
#define MODULE_STATUS_NONE 0
#define MODULE_STATUS_INIT 1
#define MODULE_STATUS_ACTIVE 2
#define MODULE_STATUS_PAUSE 4
#define MODULE_STATUS_STOP 8
#define MODULE_STATUS_DONE 0x80000000

void module_set_name(module *mod, char *name);

static inline char* module_get_name(module *mod)
{
	return mod->name;
}

void module_register_allocator(module *mod, memory_ops *ops, void *private);

static inline memory_allocator* module_get_allocator(module *mod)
{
	return mod->allocator;
}

void module_register_driver(module *mod, module_driver *driver, void *driver_data);

static inline void* module_get_driver_data(module *mod)
{
	return mod->driver_data;
}

int module_wait_status(module *mod, int status);
void module_set_status(module *mod, int status);
int module_wait_status_done(module *mod, int status);
void module_set_status_done(module *mod, int status);
void module_push_frame(module *mod, shm_buff *buff);
shm_buff* module_get_frame(module *mod);
void module_clear_frames(module *mod);
shm_buff* module_alloc_buffer(module *mod, int type, int size);
shm_buff* module_next_alloc_buffer(module *mod, int size);
shm_buff* module_prev_alloc_buffer(module *mod, int size);
bool module_if_ours_buffer(module *mod, shm_buff *buff);
int module_get_memory_type(module *mod, int cap);
module* module_create(int (*module_init)(module *));
void module_config(module *mod, char *name, unsigned long value);


/* link_path related */

typedef struct link_path {
	int status;
	module *head;
	module *tail;
	MUTEX_HANDLE(mutex);
} link_path;

link_path* link_modules(link_path *ln, module *mod);
int link_config_modules(link_path *ln, char *name, unsigned long value);
int link_init_modules(link_path *ln);
int link_active_modules(link_path *ln);
int link_pause_modules(link_path *ln);
int link_stop_modules(link_path *ln);


/* module init */
int fin_module_init(module *mod);
int fout_module_init(module *mod);
int v4l2_module_init(module *mod);
int v4l2_mdp_module_init(module *mod);
int v4l2_camera_module_init(module *mod);
int ppm_logo_module_init(struct module *mod);
int drm_module_init(module *mod);
int list2va_module_init(struct module *mod);
int va2list_module_init(struct module *mod);
int h264_logo_module_init(struct module *mod);
int v4l2_codec_module_init(module *mod);
int fourinone_camera_module_init(module *mod);
int composite_module_init(module *mod);
int v4l2_nr_module_init(module *mod);
int fake_module_init(struct module *mod);
int ovl_module_init(struct module * mod);
#endif //FASTRVC_MODULE_H
