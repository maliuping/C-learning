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
#include <string.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "module.h"
#include "queue.h"
#include "shm_buff.h"

void module_set_name(module *mod, char *name)
{
	mod->name = strdup(name);
}

void module_register_allocator(module *mod, memory_ops *ops, void *private)
{
	mod->allocator = shm_create_allocator(ops, private);
}

void module_register_driver(module *mod, module_driver *driver, void *driver_data)
{
	mod->driver = driver;
	mod->driver_data = driver_data;
}

int module_wait_status(module *mod, int status)
{
	MUTEX_LOCK(mod->mutex_status);
	while ((mod->status & status) == 0) {
		COND_WAIT(mod->cond_status, mod->mutex_status);
	}
	status &= mod->status;
	MUTEX_UNLOCK(mod->mutex_status);
	return status;
}

void module_set_status(module *mod, int status)
{
	MUTEX_LOCK(mod->mutex_status);
	mod->status = status;
	COND_BROADCAST(mod->cond_status);
	MUTEX_UNLOCK(mod->mutex_status);
}

int module_wait_status_done(module *mod, int status)
{
	MUTEX_LOCK(mod->mutex_status);
	while ((mod->status & MODULE_STATUS_DONE) == 0 ||
			(mod->status & status) == 0) {
		COND_WAIT(mod->cond_status, mod->mutex_status);
	}
	status &= mod->status;
	MUTEX_UNLOCK(mod->mutex_status);
	return status;
}

void module_set_status_done(module *mod, int status)
{
	MUTEX_LOCK(mod->mutex_status);
	mod->status = status | MODULE_STATUS_DONE;
	COND_BROADCAST(mod->cond_status);
	MUTEX_UNLOCK(mod->mutex_status);
}

#ifdef PERFORMANCE_MONITOR
static void module_update_fps(module *mod)
{
	struct timeval time_end;
	unsigned int start, end;

	mod->fps_update_count = (unsigned int)mod->output_fps * 10;
	if (mod->fps_update_count < 5)
		mod->fps_update_count = 5;

	if (mod->frame_count == 0) {
		gettimeofday(&mod->time_start, NULL);
	}
	mod->frame_count++;
	if (mod->frame_count >= mod->fps_update_count) {
		mod->frame_count = 0;
		gettimeofday(&time_end, NULL);
		start = mod->time_start.tv_sec * 1000 + mod->time_start.tv_usec / 1000;
		end = time_end.tv_sec * 1000 + time_end.tv_usec / 1000;
		mod->output_fps = mod->fps_update_count * 1000.0 / (end - start);
		if (mod->next)
			mod->next->input_fps = mod->output_fps;

		LOG_DBG("%s output fps=%f", mod->name, mod->output_fps);
	}
}
#endif

void module_push_frame(module *mod, shm_buff *buff)
{
	if (mod->next) {
		queue_enq(mod->next->q_media, buff);
#ifdef PERFORMANCE_MONITOR
		module_update_fps(mod);
#endif
	} else {
		shm_free(buff);
	}
}

shm_buff* module_get_frame(module *mod)
{
	return (shm_buff*)queue_deq(mod->q_media);
}

void module_clear_frames(module *mod)
{
	shm_buff *shm;
	bool old_mode;

	old_mode = queue_block_mode(mod->q_media, false);
	while (1) {
		shm = (shm_buff*)queue_deq(mod->q_media);
		if (shm == NULL)
			break;
		shm_free(shm);
	}
	queue_block_mode(mod->q_media, old_mode);
}

shm_buff* module_alloc_buffer(module *mod, int type, int size)
{
	return shm_alloc(mod->allocator, type, size);
}

shm_buff* module_next_alloc_buffer(module *mod, int size)
{
	if (mod->next)
		return shm_alloc(mod->next->allocator, BUFFER_INPUT, size);
	return NULL;
}

shm_buff* module_prev_alloc_buffer(module *mod, int size)
{
	if (mod->prev)
		return shm_alloc(mod->prev->allocator, BUFFER_INPUT, size);
	return NULL;
}

bool module_if_ours_buffer(module *mod, shm_buff *buff)
{
	return (mod->allocator == buff->allocator);
}

static void* module_main_task(void *data)
{
	char thread_name[256];
	module *mod;
	module_driver *driver;
	int status;

	mod = (module*)data;
	driver = mod->driver;

	module_wait_status(mod, MODULE_STATUS_INIT);
	sprintf(thread_name, "%s_main", mod->name);
	prctl(PR_SET_NAME, thread_name);

	MOD_INFO(mod, "got INIT");
	if (driver->init(mod) == 0) {
		module_set_status_done(mod, MODULE_STATUS_INIT);
	} else {
		module_set_status_done(mod, MODULE_STATUS_STOP);
		MOD_DBG(mod, "error STOP");
		return NULL;
	}
	MOD_INFO(mod, "done INIT");

	while (1) {
		status = module_wait_status(mod,
					MODULE_STATUS_ACTIVE |
						MODULE_STATUS_PAUSE |
							MODULE_STATUS_STOP);
		if (status & MODULE_STATUS_STOP) {
			MOD_DBG(mod, "got STOP");
			break;
		}
		if (status & MODULE_STATUS_PAUSE) {
			MOD_DBG(mod, "got PAUSE");
			module_set_status_done(mod, MODULE_STATUS_PAUSE);
			status = module_wait_status(mod, MODULE_STATUS_ACTIVE | MODULE_STATUS_STOP);
			if (status & MODULE_STATUS_STOP) {
				MOD_DBG(mod, "got STOP");
				break;
			}
		}

		driver->handle_frame(mod);
	}

	if (driver->deinit)
		driver->deinit(mod);
	module_set_status_done(mod, MODULE_STATUS_STOP);
	return NULL;
}

int module_get_memory_type(module *mod, int cap)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
			return mod->in_mem_type;
		case CAPS_MEM_OUTPUT:
			return mod->out_mem_type;
		default:
			return -1;
	}
	return -1;
}

module* module_create(int (*module_init)(module *))
{
	module *mod;
	mod = (module*)malloc(sizeof(*mod));
	if (!mod)
		return NULL;
	memset(mod, 0, sizeof(*mod));

	if ((module_init(mod) != 0) || (mod->driver == NULL)) {
		free(mod);
		return NULL;
	}

	if (mod->allocator && mod->allocator->ops->caps) {
		mod->allocator->ops->caps(mod->allocator->private,
			CAPS_MEM_INPUT, &mod->in_mem_type);
		mod->allocator->ops->caps(mod->allocator->private,
			CAPS_MEM_OUTPUT, &mod->out_mem_type);
	} else {
		mod->in_mem_type = MEMORY_ALLOC_SELF_OTHERS;
		mod->out_mem_type = MEMORY_ALLOC_SELF_OTHERS;
	}

	MUTEX_INIT(mod->mutex_status);
	COND_INIT(mod->cond_status);

	mod->q_media = queue_create();
	queue_block_mode(mod->q_media, true);
	queue_timeout(mod->q_media, 1);

	THREAD_CREATE(mod->thread_main, module_main_task, mod);
	return mod;
}

void module_config(module *mod, char *name, unsigned long value)
{
	LOG_DBG("CONFIG: <%s> %s=%lu", mod->name, name, value);
	if (mod->driver && mod->driver->config)
		mod->driver->config(mod, name, value);
}

link_path* link_modules(link_path *ln, module *mod)
{
	module *prev;
	if (ln == NULL) {
		ln = (link_path*)malloc(sizeof(*ln));
		if (!ln)
			return NULL;
		memset(ln, 0, sizeof(*ln));
		MUTEX_INIT(ln->mutex);
	}

	prev = ln->tail;
	if (prev) {
		if (prev->out_mem_type == MEMORY_ALLOC_SELF) {
			if (mod->in_mem_type == MEMORY_ALLOC_OTHERS ||
					mod->in_mem_type == MEMORY_ALLOC_SELF_OTHERS) {
				mod->in_mem_type = MEMORY_ALLOC_OTHERS;
			}
		} else if (prev->out_mem_type == MEMORY_ALLOC_OTHERS) {
			if (mod->in_mem_type == MEMORY_ALLOC_OTHERS) {
				// error...no body can alloc memory
				return NULL;
			} else {
				mod->in_mem_type = MEMORY_ALLOC_SELF;
			}
		} else {
			if (mod->in_mem_type == MEMORY_ALLOC_SELF) {
				prev->out_mem_type = MEMORY_ALLOC_OTHERS;
			} else if (mod->in_mem_type == MEMORY_ALLOC_OTHERS) {
				prev->out_mem_type = MEMORY_ALLOC_SELF;
			} else {
				prev->out_mem_type = MEMORY_ALLOC_OTHERS;
				mod->in_mem_type = MEMORY_ALLOC_SELF;
			}
		}
	}

	MUTEX_LOCK(ln->mutex);
	if (ln->head == NULL) {
		ln->head = mod;
		ln->tail = mod;
		mod->prev = mod->next = NULL;
	} else {
		ln->tail->next = mod;
		mod->prev = ln->tail;
		mod->next = NULL;
		ln->tail = mod;
	}
	MUTEX_UNLOCK(ln->mutex);

	return ln;
}

int link_config_modules(link_path *ln, char *name, unsigned long value)
{
	module *mod;
	int ret;
	ret = 0;

	MUTEX_LOCK(ln->mutex);
	mod = ln->head;
	while (mod) {
		module_config(mod, name, value);
		mod = mod->next;
	}
	MUTEX_UNLOCK(ln->mutex);
	return ret;
}

int link_init_modules(link_path *ln)
{
	module *mod;
	int r;
	char mem_log[1024];
	char log_temp[1024];
	r = 0;

	MUTEX_LOCK(ln->mutex);
	if (ln->status == MODULE_STATUS_INIT)
		goto ret;
	if (ln->status != MODULE_STATUS_NONE) {
		LOG_ERR("link status wrong:0x%X", ln->status);
		r = -1;
		goto ret;
	}

	mem_log[0] = 0;
	mod = ln->head;
	while (mod) {
		if (mod->prev) {
			if (mod->prev->out_mem_type == MEMORY_ALLOC_SELF)
				sprintf(log_temp, "[%s:S]<=", mod->prev->name);
			else
				sprintf(log_temp, "[%s:O]=", mod->prev->name);
			strcat(mem_log, log_temp);

			if (mod->in_mem_type == MEMORY_ALLOC_SELF)
				sprintf(log_temp, "=>[%s:S]", mod->name);
			else
				sprintf(log_temp, "=[%s:O]", mod->name);
			strcat(mem_log, log_temp);
		}
		mod = mod->next;
	}
	LOG_INFO("link memory: %s", mem_log);

	mod = ln->head;
	while (mod) {
		module_set_status(mod, MODULE_STATUS_INIT);
		mod = mod->next;
	}
/*
	mod = ln->head;
	while (mod) {
		status = module_wait_status_done(mod,
					MODULE_STATUS_INIT | MODULE_STATUS_STOP);
		if (status & MODULE_STATUS_STOP) {
			LOG_ERR("module %s stoped", mod->name);
			r = -1;
			goto ret;
		}
		mod = mod->next;
	}
*/
	ln->status = MODULE_STATUS_INIT;
ret:
	MUTEX_UNLOCK(ln->mutex);
	return r;
}

int link_active_modules(link_path *ln)
{
	module *mod;
	int status;
	int r;
	r = 0;

	MUTEX_LOCK(ln->mutex);
	if (ln->status == MODULE_STATUS_ACTIVE)
		goto ret;

	if (ln->status == MODULE_STATUS_INIT) {
		mod = ln->head;
		while (mod) {
			MOD_DBG(mod, "wait INIT");
			status = module_wait_status_done(mod,
						MODULE_STATUS_INIT | MODULE_STATUS_STOP);
			if (status & MODULE_STATUS_STOP) {
				LOG_ERR("module %s stoped", mod->name);
				ln->status = MODULE_STATUS_STOP;
				r = -1;
				goto ret;
			}
			mod = mod->next;
		}
	} else if (ln->status != MODULE_STATUS_PAUSE) {
		LOG_ERR("link status wrong:0x%X", ln->status);
		r = -1;
		goto ret;
	}

	mod = ln->head;
	while (mod) {
		module_set_status(mod, MODULE_STATUS_ACTIVE);
		mod = mod->next;
	}

	ln->status = MODULE_STATUS_ACTIVE;
ret:
	MUTEX_UNLOCK(ln->mutex);
	return r;
}

int link_pause_modules(link_path *ln)
{
	module *mod;
	int status;
	int r;
	r = 0;

	MUTEX_LOCK(ln->mutex);
	if (ln->status == MODULE_STATUS_PAUSE)
		goto ret;
	if (ln->status != MODULE_STATUS_INIT
		&& ln->status != MODULE_STATUS_ACTIVE) {
		LOG_ERR("link status wrong:0x%X, link->head->module is %s", ln->status, module_get_name(ln->head) );
		r = -1;
		goto ret;
	}

	if (ln->status == MODULE_STATUS_INIT) {
		mod = ln->head;
		while (mod) {
			MOD_DBG(mod, "wait INIT");
			status = module_wait_status_done(mod,
						MODULE_STATUS_INIT | MODULE_STATUS_STOP);
			if (status & MODULE_STATUS_STOP) {
				LOG_ERR("module %s stoped", mod->name);
				ln->status = MODULE_STATUS_STOP;
				r = -1;
				goto ret;
			}
			mod = mod->next;
		}
	}

	mod = ln->head;
	while (mod) {
		module_set_status(mod, MODULE_STATUS_PAUSE);
		mod = mod->next;
	}

	mod = ln->head;
	while (mod) {
		MOD_DBG(mod, "wait PAUSE");
		module_wait_status_done(mod, MODULE_STATUS_PAUSE);
		module_clear_frames(mod);
		if (mod->driver->pause) {
			mod->driver->pause(mod);
		}
		mod = mod->next;
	}

	ln->status = MODULE_STATUS_PAUSE;
ret:
	MUTEX_UNLOCK(ln->mutex);
	return r;
}

int link_stop_modules(link_path *ln)
{
	module *mod;
	int status;
	int r;
	r = 0;

	MUTEX_LOCK(ln->mutex);
	if (ln->status == MODULE_STATUS_STOP
		|| ln->status == MODULE_STATUS_NONE)
		goto ret;

	if (ln->status == MODULE_STATUS_ACTIVE) {
		//MUTEX_UNLOCK(ln->mutex);
		//link_pause_modules(ln); // avoid buffer conflict error, pause first if not
		//MUTEX_LOCK(ln->mutex);

		mod = ln->head;
		while (mod) {
			module_set_status(mod, MODULE_STATUS_PAUSE);
			mod = mod->next;
		}

		mod = ln->head;
		while (mod) {
			MOD_DBG(mod, "wait PAUSE");
			module_wait_status_done(mod, MODULE_STATUS_PAUSE);
			module_clear_frames(mod);
			//if (mod->driver->pause) {
			//	mod->driver->pause(mod);
			//}
			mod = mod->next;
		}

		ln->status = MODULE_STATUS_PAUSE;
	} else if (ln->status == MODULE_STATUS_INIT) {
		mod = ln->head;
		while (mod) {
			MOD_DBG(mod, "wait INIT");
			status = module_wait_status_done(mod,
						MODULE_STATUS_INIT | MODULE_STATUS_STOP);
			if (status & MODULE_STATUS_STOP) {
				LOG_ERR("module %s stoped", mod->name);
				ln->status = MODULE_STATUS_STOP;
				r = -1;
				goto ret;
			}
			mod = mod->next;
		}
	}

	mod = ln->head;
	while (mod) {
		module_set_status(mod, MODULE_STATUS_STOP);
		mod = mod->next;
	}
	mod = ln->head;
	while (mod) {
		MOD_DBG(mod, "wait STOP");
		module_wait_status_done(mod, MODULE_STATUS_STOP);
		mod = mod->next;
	}

	ln->status = MODULE_STATUS_STOP;
ret:
	MUTEX_UNLOCK(ln->mutex);
	return r;
}
