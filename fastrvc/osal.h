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
#ifndef FASTRVC_OSAL_H
#define FASTRVC_OSAL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef gettid
#include <unistd.h>
#include <sys/syscall.h>
#define gettid() syscall(__NR_gettid)
#endif

#ifndef bool
#define bool int
#endif
#ifndef false
#define false 0
#define true (!false)
#endif

#define MUTEX_HANDLE(mutex_handle) pthread_mutex_t mutex_handle
#define MUTEX_INIT(mutex_handle) pthread_mutex_init(&mutex_handle, NULL)
#define MUTEX_LOCK(mutex_handle) pthread_mutex_lock(&mutex_handle)
#define MUTEX_UNLOCK(mutex_handle) pthread_mutex_unlock(&mutex_handle)
#define MUTEX_DESTROY(mutex_handle) pthread_mutex_destroy(&mutex_handle)
#define COND_HANDLE(cond_handle) pthread_cond_t cond_handle
#define COND_INIT(cond_handle) pthread_cond_init(&cond_handle, NULL)
#define COND_WAIT(cond_handle, mutex_handle) pthread_cond_wait(&cond_handle, &mutex_handle)
#define COND_SIGNAL(cond_handle) pthread_cond_signal(&cond_handle)
#define COND_BROADCAST(cond_handle) pthread_cond_broadcast(&cond_handle)
#define COND_DESTROY(cond_handle) pthread_cond_destroy(&cond_handle)
#define SEM_HANDLE(sem) sem_t sem
#define SEM_INIT(sem, init) sem_init(&sem, 0, init)
#define SEM_WAIT(sem) sem_wait(&sem)
#define SEM_TIMED_WAIT(sem, ms) \
	do {\
		struct timespec ts;\
		clock_gettime(CLOCK_REALTIME, &ts);\
		ts.tv_nsec += (ms * 1000000);\
		ts.tv_sec += (ts.tv_nsec / 1000000000);\
		ts.tv_nsec = ts.tv_nsec % 1000000000;\
		sem_timedwait(&sem, &ts);\
	} while (0)
#define SEM_POST(sem) sem_post(&sem)
#define SEM_DESTROY(sem) sem_destroy(&sem)
#define THREAD_HANDLE(thread_handle) pthread_t thread_handle
#define THREAD_CREATE(thread_handle, task, data) \
	do { \
		pthread_attr_t attr; \
		pthread_attr_init(&attr); \
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); \
		pthread_create(&thread_handle, &attr, task, data); \
	} while (0)
#define THREAD_WAIT(thread_handle) pthread_join(thread_handle, NULL)


#define LOG_LEVEL_CRITICAL 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4
#define LOG_LEVEL_VERBOSE 5
#define PRINT_LOG_LEVEL LOG_LEVEL_INFO
extern int dynamic_log_level;

#define LOG_FUNC_NAME 1
#define LOG_THREAD_NAME 1

#ifdef LOG_FUNC_NAME
#define LOG_FUNC_TAG "[%s:%d]"
#define LOG_FUNC_INFO , __func__, __LINE__
#else
#define LOG_FUNC_TAG
#define LOG_FUNC_INFO
#endif

#ifdef LOG_THREAD_NAME
#include <sys/prctl.h>
#define GET_THREAD_INFO \
	char thread_name[17];\
	do {\
		memset(thread_name, 0, sizeof(thread_name));\
		prctl(PR_GET_NAME, thread_name);\
	} while (0)
#define LOG_THREAD_TAG "(%s)"
#define LOG_THREAD_INFO , thread_name
#else
#define GET_THREAD_INFO
#define LOG_THREAD_TAG
#define LOG_THREAD_INFO
#endif

#define LOG_PRINT(loglevel, tag, fmt, args...)\
	do {\
		struct timespec ts;\
		if (dynamic_log_level >= 0) {\
			if (loglevel > dynamic_log_level)\
				break;\
		} else if (loglevel > PRINT_LOG_LEVEL) {\
			break;\
		}\
		GET_THREAD_INFO;\
		clock_gettime(CLOCK_BOOTTIME, &ts);\
		printf("[%5lu.%6lu]" LOG_THREAD_TAG "[MTK_RVC]" tag LOG_FUNC_TAG fmt "\n",\
				ts.tv_sec, ts.tv_nsec / 1000\
				LOG_THREAD_INFO LOG_FUNC_INFO, ##args);\
		fflush(stdout);\
	} while (0)
#define LOG_CRIT(fmt, args...) LOG_PRINT(LOG_LEVEL_CRITICAL, "[CRIT]", fmt, ##args)
#define LOG_ERR(fmt, args...) LOG_PRINT(LOG_LEVEL_ERROR, "[ERROR]", fmt, ##args)
#define LOG_WARN(fmt, args...) LOG_PRINT(LOG_LEVEL_WARNING, "[WARN]", fmt, ##args)
#define LOG_INFO(fmt, args...) LOG_PRINT(LOG_LEVEL_INFO, "[INFO]", fmt, ##args)
#define LOG_DBG(fmt, args...) LOG_PRINT(LOG_LEVEL_DEBUG, "[DEBUG]", fmt, ##args)

#define LOG_VAR(x, y) x ## y
#define MAX_VERBOSE_THREADS 10
#define LOG_VERBOSE(count, fmt, args...) \
	do {\
		int i;\
		unsigned int current_tid;\
		static unsigned int LOG_VAR(thread_count_, __LINE__) = 0;\
		static unsigned int LOG_VAR(log_tid_, __LINE__)[MAX_VERBOSE_THREADS] = {0};\
		static unsigned int LOG_VAR(log_count_, __LINE__)[MAX_VERBOSE_THREADS] = {0};\
		if (dynamic_log_level >= 0) {\
			if (LOG_LEVEL_VERBOSE > dynamic_log_level)\
				break;\
		} else if (LOG_LEVEL_VERBOSE > PRINT_LOG_LEVEL) {\
			break;\
		}\
		current_tid = gettid();\
		for (i = 0; i < LOG_VAR(thread_count_, __LINE__); i++) {\
			if (LOG_VAR(log_tid_, __LINE__)[i] == current_tid)\
				break;\
		}\
		if (i == LOG_VAR(thread_count_, __LINE__)) {\
			if (i < MAX_VERBOSE_THREADS) {\
				LOG_VAR(thread_count_, __LINE__)++;\
				LOG_VAR(log_tid_, __LINE__)[i] = current_tid;\
			} else {\
				break;\
			}\
		}\
		if (LOG_VAR(log_count_, __LINE__)[i] == 0) {\
			LOG_PRINT(LOG_LEVEL_VERBOSE, "[VERBOSE]", "(%d)" fmt, count, ##args);\
		}\
		LOG_VAR(log_count_, __LINE__)[i]++;\
		if (LOG_VAR(log_count_, __LINE__)[i] >= (count)) {\
			LOG_VAR(log_count_, __LINE__)[i] = 0;\
		}\
	} while(0)

#define LOG_ONCE(fmt, args...) \
	do {\
		static unsigned int LOG_VAR(log_count_, __LINE__) = 0;\
		if (LOG_VAR(log_count_, __LINE__) == 0) {\
			LOG_VAR(log_count_, __LINE__) = 1;\
			LOG_PRINT(LOG_LEVEL_INFO, "[ONCE]", fmt, ##args);\
		}\
	} while(0)

#define PERFORMANCE_MONITOR 1
#define UPDATE_FPS_COUNT 200
/* !!!NOTE: LOG_FPS just can be used in module's private code */
#define LOG_FPS(fmt, args...) \
	do {\
		struct timeval fps_end;\
		double fps;\
		unsigned int time_start, time_end;\
		static unsigned int LOG_VAR(fps_count_, __LINE__) = 0;\
		static struct timeval LOG_VAR(fps_start_, __LINE__);\
		if (LOG_VAR(fps_count_, __LINE__) == 0) {\
			gettimeofday(&LOG_VAR(fps_start_, __LINE__), NULL);\
		}\
		LOG_VAR(fps_count_, __LINE__)++;\
		if (LOG_VAR(fps_count_, __LINE__) >= (UPDATE_FPS_COUNT)) {\
			LOG_VAR(fps_count_, __LINE__) = 0;\
			gettimeofday(&fps_end, NULL);\
			time_start = LOG_VAR(fps_start_, __LINE__).tv_sec * 1000 + LOG_VAR(fps_start_, __LINE__).tv_usec / 1000;\
			time_end = fps_end.tv_sec * 1000 + fps_end.tv_usec / 1000;\
			fps = UPDATE_FPS_COUNT * 1000.0 / (time_end - time_start);\
			LOG_DBG(fmt " fps=%f", ##args, fps);\
		}\
	} while(0)

#ifndef LOG_BOOT_PROFILE_KERNEL
#define LOG_BOOT_PROFILE_KERNEL 1
#endif
#if LOG_BOOT_PROFILE_KERNEL
#define LOG_BOOT_PROFILE(fmt, args...) \
	do {\
		FILE *fd;\
		struct timespec ts;\
		static unsigned int LOG_VAR(log_boot_prof_, __LINE__) = 0;\
		if (LOG_VAR(log_boot_prof_, __LINE__) == 1)\
			break;\
		LOG_VAR(log_boot_prof_, __LINE__) = 1;\
		clock_gettime(CLOCK_BOOTTIME, &ts);\
		fd = fopen("/dev/kmsg", "w");\
		if (!fd) {\
			LOG_ERR("open /dev/kmsg fail");\
			break;\
		}\
		fprintf(fd, "[%5lu.%6lu][MTK_RVC][BOOT_PROF]" fmt "\n",\
					ts.tv_sec, ts.tv_nsec / 1000, ##args);\
		fclose(fd);\
	} while(0)
#else
#define LOG_BOOT_PROFILE LOG_ONCE
#endif

#endif //FASTRVC_OSAL_H
