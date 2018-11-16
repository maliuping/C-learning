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
#include "queue.h"
#include "osal.h"

queue* queue_create()
{
	queue *q;
	q = (queue*)malloc(sizeof(*q));
	if (q == NULL) {
		LOG_ERR("malloc fail");
		return NULL;
	}

	memset(q, 0, sizeof(*q));
	MUTEX_INIT(q->mutex);
	SEM_INIT(q->sem, 0);

	q->timeout = 5;
	return q;
}

void queue_destroy(queue *q)
{
	q_node *node, *next;
	if (!q)
		return;

	MUTEX_LOCK(q->mutex);
	next = q->head;
	while (next) {
		node = next;
		next = next->next;
		free(node);
	}
	q->head = NULL;
	q->tail = NULL;
	MUTEX_UNLOCK(q->mutex);

	MUTEX_DESTROY(q->mutex);
	SEM_DESTROY(q->sem);
	free(q);
}

void queue_timeout(queue *q, unsigned int timeout_ms)
{
	MUTEX_LOCK(q->mutex);
	q->timeout = timeout_ms;
	MUTEX_UNLOCK(q->mutex);
}

bool queue_block_mode(queue *q, bool block_mode)
{
	bool old;
	MUTEX_LOCK(q->mutex);
	old = q->block_mode;
	q->block_mode = block_mode;
	MUTEX_UNLOCK(q->mutex);
	return old;
}

void queue_enq(queue *q, void *data)
{
	q_node *node;
	node = (q_node*)malloc(sizeof(*node));
	if (node == NULL) {
		LOG_ERR("malloc fail");
		return;
	}
	memset(node, 0, sizeof(*node));
	node->data = data;

	MUTEX_LOCK(q->mutex);
	if (q->head == NULL) {
		q->head = node;
		q->tail = node;
	} else {
		q->tail->next = node;
		q->tail = node;
	}
	MUTEX_UNLOCK(q->mutex);

	if (q->block_mode) {
		SEM_POST(q->sem);
	}
}

void queue_enq_head(queue *q, void *data)
{
	q_node *node;
	node = (q_node*)malloc(sizeof(*node));
	if (node == NULL) {
		LOG_ERR("malloc fail");
		return;
	}
	memset(node, 0, sizeof(*node));
	node->data = data;

	MUTEX_LOCK(q->mutex);
	if (q->head == NULL) {
		q->head = node;
		q->tail = node;
	} else {
		node->next = q->head;
		q->head = node;
	}
	MUTEX_UNLOCK(q->mutex);

	if (q->block_mode) {
		SEM_POST(q->sem);
	}
}
void* queue_deq(queue *q)
{
	q_node *node;
	void *data;

	if (q->block_mode) {
		SEM_TIMED_WAIT(q->sem, q->timeout);
	}

	MUTEX_LOCK(q->mutex);
	node = q->head;
	if (q->head) {
		q->head = q->head->next;
		node->next = NULL;
	}
	MUTEX_UNLOCK(q->mutex);

	data = NULL;
	if (node) {
		data = node->data;
		free(node);
	}
	return data;
}

bool queue_empty(queue *q)
{
	return (q->head == NULL);
}