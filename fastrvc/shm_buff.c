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
#define CAPS_MEM_INPUT 0
#define CAPS_MEM_OUTPUT 1

#include <stdlib.h>
#include <sys/prctl.h>
#include <malloc.h>
#include <unistd.h>
#include "osal.h"
#include "shm_buff.h"
#include "queue.h"

static bool inited = 0;
static struct queue *free_queue;
static THREAD_HANDLE(shm_thread);

static void* shm_buffer_free_task(void *data)
{
	queue *q;
	shm_buff *shm;
	prctl(PR_SET_NAME, "shm_task");

	q = (queue*)data;
	while (1) {
		shm = queue_deq(q);
		if (shm)
			shm_free(shm);
		usleep(1000);
	}
	return NULL;
}

int shm_init()
{
	if (inited)
		return 0;

	free_queue = queue_create();
	queue_block_mode(free_queue, true);
	THREAD_CREATE(shm_thread, shm_buffer_free_task, free_queue);
	inited = 1;
	return 0;
}

int default_mem_caps(void *private, int cap, int *val)
{
	(void)private;

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

void* default_mem_alloc(void *private, int type, int *size)
{
	(void)private;
	(void)type;
	return memalign(128, *size);
}

int default_mem_free(void *private, void *buff)
{
	(void)private;
	free(buff);
	return 0;
}

void* default_mem_get_ptr(void *private, void *buff)
{
	(void)private;
	return buff;
}

static memory_ops default_memory_ops = {
	.caps = default_mem_caps,
	.alloc = default_mem_alloc,
	.free = default_mem_free,
	.get_ptr = default_mem_get_ptr,
};

static memory_allocator default_memory_allocator = {
	.private = NULL,
	.ops = &default_memory_ops,
};

memory_allocator* shm_create_allocator(memory_ops *ops, void *private)
{
	memory_allocator *allocator;
	allocator = (memory_allocator*)malloc(sizeof(memory_allocator));
	if (allocator == NULL)
		return NULL;
	allocator->ops = ops;
	allocator->private = private;
	return allocator;
}

void shm_destroy_allocator(memory_allocator *allocator)
{
	free(allocator);
}

shm_buff* shm_alloc(memory_allocator *allocator, int type, int size)
{
	shm_buff *shm;

	shm = (shm_buff*)malloc(sizeof(shm_buff));
	if (shm == NULL)
		return NULL;
	memset(shm, 0, sizeof(*shm));

	if (allocator == NULL) {
		shm->allocator = &default_memory_allocator;
	} else {
		shm->allocator = allocator;
	}

	shm->size = size;
	shm->buff = shm->allocator->ops->alloc(shm->allocator->private, type, &shm->size);
	if (shm->buff == NULL) {
		free(shm);
		return NULL;
	}

	if (shm->size < size)
		LOG_WARN("be careful, allocated(%d) < required(%d)", shm->size, size);

	return shm;
}

shm_buff* shm_dup_buff(memory_allocator *allocator, void *buff)
{
	shm_buff *shm;
	shm = (shm_buff*)malloc(sizeof(shm_buff));
	if (shm == NULL)
		return NULL;
	memset(shm, 0, sizeof(*shm));

	if (allocator == NULL) {
		shm->allocator = &default_memory_allocator;
	} else {
		shm->allocator = allocator;
	}
	shm->buff = buff;
	return shm;
}

void shm_free(shm_buff *shm)
{
	if (shm->allocator->ops->free(shm->allocator->private, shm->buff) == 0) {
		free(shm);
	} else if(inited) {
		queue_enq(free_queue, shm);
	} else {
		free(shm);
	}
}

void* shm_get_ptr(shm_buff *shm)
{
	return shm->allocator->ops->get_ptr(shm->allocator->private, shm->buff);
}

#ifdef SHM_DEBUG_CRC
long shm_calc_crc(void *buff, int size)
{
	long crc;
	int i, step;
	long *val;

	step = sizeof(long);
	val = (long*)buff;
	for (i = 0, crc = 0; i < size; i += step) {
		crc += *val;
		val++;
	}
	crc |= 0x01; // set crc valid
	return crc;
}
#endif
