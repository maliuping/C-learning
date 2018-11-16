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
#include "module.h"


/*******************************module driver**************************/

static int fake_mem_caps(void *private, int cap, int *val)
{
	switch (cap) {
		case CAPS_MEM_INPUT:
		case CAPS_MEM_OUTPUT:
			*val = MEMORY_ALLOC_OTHERS;
			break;
		default:
			return -1;
	}
	return 0;
}

static memory_ops fake_mem_ops = {
	.caps = fake_mem_caps,
	.alloc = NULL,
	.free = NULL,
	.get_ptr = NULL,
};

static void fake_driver_config(struct module *mod, char *name, unsigned long value)
{
}

static int fake_driver_init(struct module *mod)
{
	return 0;
}

static void fake_driver_deinit(struct module *mod)
{
}

static int fake_driver_handle_frame(struct module *mod)
{
	struct shm_buff *frame;
	frame = module_get_frame(mod);
	if (frame)
		shm_free(frame);

	return 0;
}

module_driver fake_driver = {
	.config = fake_driver_config,
	.init = fake_driver_init,
	.deinit = fake_driver_deinit,
	.handle_frame = fake_driver_handle_frame,
};

int fake_module_init(struct module *mod)
{
	module_set_name(mod, "fake");
	module_register_driver(mod, &fake_driver, NULL);
	module_register_allocator(mod, &fake_mem_ops, NULL);
	return 0;
}
