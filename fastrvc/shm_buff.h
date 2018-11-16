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
#ifndef FASTRVC_SHM_BUFF_H
#define FASTRVC_SHM_BUFF_H

typedef struct memory_ops {
	/*
	* int caps(void *private, int cap, int *val)
	* query allocator's capability
	*/
	int (*caps)(void *, int, int *);
	/*
	* void* alloc(void *private, int type, int *size)
	*/
	void* (*alloc)(void *, int, int *);
	/*
	* int free(void *private, void *buff)
	*/
	int (*free)(void *, void *);
	/*
	* void* get_ptr(void *private, void *buff)
	* transform 'buff' to data address
	*/
	void* (*get_ptr)(void *, void *);
}memory_ops;

/* flags for 'caps->cap' */
#define CAPS_MEM_INPUT 0
#define CAPS_MEM_OUTPUT 1
/* flags for 'caps->val' */
#define MEMORY_ALLOC_SELF 0			/* memory must be allocated by self module */
#define MEMORY_ALLOC_OTHERS 1		/* memory must be allocated by other module */
#define MEMORY_ALLOC_SELF_OTHERS 2	/* memory can be allocated both self and other */

typedef struct memory_allocator {
	void *private;
	memory_ops *ops;
} memory_allocator;

typedef struct shm_buff {
	memory_allocator *allocator;
	void *buff;
	int size;
#ifdef SHM_DEBUG_CRC
	long crc; // bit0 set 1 means crc valid
#endif
} shm_buff;

int shm_init();

#define BUFFER_INPUT 0
#define BUFFER_OUTPUT 1

int default_mem_caps(void *private, int cap, int *val);
void* default_mem_alloc(void *private, int type, int *size);
int default_mem_free(void *private, void *buff);
void* default_mem_get_ptr(void *private, void *buff);
memory_allocator* shm_create_allocator(memory_ops *ops, void *private);
void shm_destroy_allocator(memory_allocator *allocator);
shm_buff* shm_alloc(memory_allocator *allocator, int type, int size);
shm_buff* shm_dup_buff(memory_allocator *allocator, void *buff);
void shm_free(shm_buff *shm);
void* shm_get_ptr(shm_buff *shm);

#ifdef SHM_DEBUG_CRC
static inline bool shm_crc_valid(shm_buff *shm)
{
	return (bool)(shm->crc & 0x01);
}
long shm_calc_crc(void *buff, int size);
#endif

#endif //FASTRVC_SHM_BUFF_H