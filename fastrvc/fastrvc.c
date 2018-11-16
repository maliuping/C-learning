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
#include <sys/prctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include "module.h"
#include "device_config.h"
#include "drm_display.h"
#include "util.h"
#include "xml.h"
#include "cpp_util.h"
#include "FastrvcDef.h"
#include "backlight.h"

#define RVC_MAX_TIMES    200
#define RVC_SLEEP_TIMES    50

static link_path *logo_link;
static link_path *camera_link;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

MUTEX_HANDLE(mutex_link);
static bool is_home_ready(void *xml_handle);
static void* camera_thread(void *data);
static void* logo_thread(void *data);
static void* rvc_detect_thread(void *data);
static void* listener_thread(void *data);
static void* recovery_backlight_thread(void *data);
static void* recovery_keybacklight_thread(void *data);
static void sig_handler(int sig);
static bool check_rvc();

static void help_cmd();

struct camera_thread_parameter {
	void * handle;
	bool in_kernel_domain;
};

struct cmdline_options {
	bool first_stage;
	bool logo;
};
static int parse_cmdline(int argc, char *argv[], struct cmdline_options *opt);

void import_kernel_cmdline(void (*import_kernel_nv)(char *name));

static void import_kernel_nv(char *name);
static void process_kernel_cmdline(void);
static bool is_camera_display();

static bool check_data(struct data_info_item *pinfo);
static bool check_data_other(struct data_info_item *pinfo );
static int get_data(struct eMMC_data* info);
static int get_other(struct other_data* other);
static int read_data_info(struct data_info_item *pinfo,uint32_t seek);
static int convertToBrightness(bool dayNightFlag, int brightness_step);
static int convertToContrast(bool rgbOrCameraFlag, int contrast_step);
static int write_int(char const* path, int value);
static int drm_res_init(int fd);
static int setParam(DISP_PQ_PARAM *pqParam);
static int setIndex(const DISPLAY_PQ_T *pqIndex);
static bool check_ILL();

static struct eMMC_data user_def = {0,0,0,0,0,0};
static struct other_data other_def = {1,0};
static int backlight_step[34] = {40,61,81,102,122,143,163,184,204,224,245,266,286,307,327,348,368,   //NIGHT
                                 225,276,327,378,430,481,532,583,634,688,737,788,839,890,942,983,1023}; //DAY
static int BootMode = 0;
int drm_fd;
unsigned int crtc_id;
unsigned int prop_id[2];

static void import_kernel_nv(char *name)
{
    char *value = strchr(name, '=');
    int name_len = strlen(name);

    if (value == 0) return;
    *value++ = 0;
    if (name_len == 0) return;

   if (!strncmp(name, "androidboot.iauto.bootmode", 26) && name_len > 26) {
       BootMode = atoi(value);
       LOG_INFO("BootMode is %d",BootMode);
    }
}


static void process_kernel_cmdline(void)
{
    /* don't expose the raw commandline to nonpriv processes */
    chmod("/proc/cmdline", 0440);

    import_kernel_cmdline(import_kernel_nv);
}



void import_kernel_cmdline(void (*import_kernel_nv)(char *name))
{
    char cmdline[1024];
    char *ptr;
    int fd;

    fd = open("/proc/cmdline", O_RDONLY);
    if (fd >= 0) {
        int n = read(fd, cmdline, 1023);
        if (n < 0) n = 0;

        /* get rid of trailing newline, it happens */
        if (n > 0 && cmdline[n-1] == '\n') n--;

        cmdline[n] = 0;
        close(fd);
    } else {
        cmdline[0] = 0;
    }

    ptr = cmdline;
    while (ptr && *ptr) {
        char *x = strchr(ptr, ' ');
        if (x != 0)
           *x++ = 0;
        import_kernel_nv(ptr);
        ptr = x;
    }
}


#define USLEEP_UNIT 100000 // 100ms

#ifdef FASTRVC_STATIC_LOGO_SUPPORT
#define STATIC_LOGO_COUNT 10
#else
#define STATIC_LOGO_COUNT 0
#endif

#ifdef FASTRVC_ANIMATED_LOGO_SUPPORT
#define ANIMATED_LOGO_COUNT 20
#else
#define ANIMATED_LOGO_COUNT 0
#endif

#define TOTAL_COUNT (STATIC_LOGO_COUNT + ANIMATED_LOGO_COUNT);

int main(int argc, char *argv[])
{
	THREAD_HANDLE(camera_handle);
	THREAD_HANDLE(rvc_handle);
	THREAD_HANDLE(listener_handle);
	THREAD_HANDLE(backlight_handle);
	THREAD_HANDLE(keybacklight_handle);
	void *xml_handle;
	struct cmdline_options cmd_opt;
	struct camera_thread_parameter camera_thread_data;

	memset(&cmd_opt, 0, sizeof(cmd_opt));
	memset(&camera_thread_data, 0, sizeof(camera_thread_data));

	LOG_INFO("fastrvc main start");
	if (parse_cmdline(argc, argv, &cmd_opt) != 0) {
		help_cmd();
		return -1;
	}

	xml_handle = xml_open(argv[1]);
	if (!xml_handle) {
		LOG_ERR("open setting file fail");
		return -1;
	}

	log_level_init(xml_handle);
	device_path_init();

	logo_link = NULL;
	camera_link = NULL;

	/* signal handler */
	struct sigaction act;
	act.sa_handler = sig_handler;
	sigaction(SIGINT, &act, NULL);

	camera_thread_data.handle = xml_handle;
	camera_thread_data.in_kernel_domain = false;
	/* init */
	initCppUtil();
	MUTEX_INIT(mutex_link);
	THREAD_CREATE(backlight_handle, recovery_backlight_thread, NULL);
	THREAD_CREATE(rvc_handle, rvc_detect_thread, xml_handle);
	THREAD_CREATE(keybacklight_handle,recovery_keybacklight_thread,NULL);
#ifdef FASTRVC_CAMERA_SUPPORT
	THREAD_CREATE(camera_handle, camera_thread, &camera_thread_data);
#endif

	THREAD_CREATE(listener_handle, listener_thread, NULL);

	// we run logo_thread in main thread
	if (cmd_opt.logo)
		logo_thread(xml_handle);

#ifdef FASTRVC_CAMERA_SUPPORT
	THREAD_WAIT(camera_handle);
#endif
	xml_close(xml_handle);
	LOG_INFO("fastrvc main exit");
	if (cmd_opt.first_stage) {
		for(int i=2; i<argc; i++)
			argv[i] = NULL;
		LOG_INFO("re-execute fastrvc");
		execve(argv[0], argv, NULL);
		LOG_INFO("re-execute fastrvc error, errno is %s", strerror(errno));
	}
	return 0;
}

static void help_cmd()
{
	printf("initfastrvc <config.xml> [--logo | --first_stage]\n");
}

static int parse_cmdline(int argc, char *argv[], struct cmdline_options *opt)
{
	if (argc < 2)
		return -1;
	if (!strstr(argv[1], "xml"))
		return -1;

	for (int i=2; i<argc; i++) {
		if(strcmp(argv[i], "--first_stage") == 0) {
			opt->first_stage = true;
		} else if(strcmp(argv[i], "--logo") == 0) {
			opt->logo = true;
		}
	}
	return 0;
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
			MUTEX_LOCK(mutex_link);
			if (logo_link)
				link_stop_modules(logo_link);
			if (camera_link)
				link_stop_modules(camera_link);
			MUTEX_UNLOCK(mutex_link);
			exit(0);
			break;
	}
}

static volatile bool pause_logo_requested = false;

static void pause_logo()
{
	MUTEX_LOCK(mutex_link);
	if (logo_link != NULL)
		link_pause_modules(logo_link);
	MUTEX_UNLOCK(mutex_link);
}

static void* logo_thread(void *data)
{
	link_path *static_logo;
	link_path *animated_logo;

	int count = 0;
	LOG_INFO("logo start");

	static_logo = NULL;
	animated_logo = NULL;

	if (BootMode != 0)
		goto end_logo;

#ifdef FASTRVC_STATIC_LOGO_SUPPORT
	static_logo = xml_create_static(data);

	if (static_logo == NULL) {
		LOG_ERR("static logo create fail");
		goto end_logo;
	}
	if (0 != link_init_modules(static_logo)) {
		LOG_ERR("static logo init fail");
		goto end_logo;
	}
#endif

#ifdef FASTRVC_ANIMATED_LOGO_SUPPORT
	animated_logo = xml_create_animated(data);

	if (animated_logo == NULL) {
		LOG_ERR("animated logo create fail");
		goto end_logo;
	}
	if (0 != link_init_modules(animated_logo)) {
		LOG_ERR("animated logo init fail");
		goto end_logo;
	}
#endif

#ifdef FASTRVC_STATIC_LOGO_SUPPORT
	/* static logo */
	MUTEX_LOCK(mutex_link);
	logo_link = static_logo;
	MUTEX_UNLOCK(mutex_link);

	if ((check_rvc() == true) || (pause_logo_requested == true))
		goto end_logo;
	if (0 != link_active_modules(static_logo))
		goto end_static;
	LOG_DBG("static logo active");
	while(access("/data/vendor/bootFinished",F_OK)){
        usleep(STATIC_LOGO_COUNT * USLEEP_UNIT);
        count++;
        if(count >= 60){
            LOG_ERR("TIME OUT");
            break;
        }
	}
end_static:
	link_pause_modules(static_logo);
	MUTEX_LOCK(mutex_link);
	logo_link = NULL;
	for(int i = 0;i<3;i++){
        int ret = unlink("/data/vendor/bootFinished");
        if (ret == 0)
            break;
	}
	MUTEX_UNLOCK(mutex_link);
#endif

#ifdef FASTRVC_ANIMATED_LOGO_SUPPORT
	/* animated logo */
	MUTEX_LOCK(mutex_link);
	logo_link = animated_logo;
	MUTEX_UNLOCK(mutex_link);

	//if ((check_rvc() == true) || (pause_logo_requested == true))
		//goto end_logo;
	while(check_rvc() == true)
		usleep(10000);
	if(is_home_ready(data) == false){
		if (0 != link_active_modules(animated_logo))
			goto end_animated;
		LOG_DBG("animated logo active");
		usleep(ANIMATED_LOGO_COUNT * USLEEP_UNIT);
	} else {
		goto end_animated;
	}
	// we will show logo until weston is ready
	while (isStopAnimatedLogo() == false)
		usleep(USLEEP_UNIT);
	usleep(USLEEP_UNIT * 9);
end_animated:
	link_pause_modules(animated_logo);
	MUTEX_LOCK(mutex_link);
	logo_link = NULL;
	MUTEX_UNLOCK(mutex_link);
#endif

end_logo:
	MUTEX_LOCK(mutex_link);
	logo_link = NULL;
	MUTEX_UNLOCK(mutex_link);

	if (static_logo)
		link_stop_modules(static_logo);
	if (animated_logo)
		link_stop_modules(animated_logo);
	LOG_INFO("logo done");
	return NULL;
}

static bool is_home_ready(void *xml_handle)
{
	DIR *dir;
	struct dirent *next;
	char name[100];
	char path[100];
	FILE *fd;
	int len;
	int pid;
	static bool ready = false;
	static char *home_daemon = NULL;

	if (ready == true)
		return true;

	if (!home_daemon) {
		/*
		*	<setting>
		*		<home_daemon value="name"/>
		*	</setting>
		*/
		home_daemon = xml_get_setting(xml_handle, "home_daemon");
		if (!home_daemon) {
			LOG_WARN("cannot get home_daemon from setting, we will not wait");
			ready = true;
			return ready;
		}
	}

	dir = opendir("/proc/");
	if (!dir) {
		LOG_ERR("opendir fail");
		return false;
	}

	while (1) {
		memset(path, '\0', sizeof(path));
		memset(name, '\0', sizeof(name));
		next = readdir(dir);
		if (!next)
			break;
		if (isdigit(next->d_name[0])) {
			pid = atoi(next->d_name);

			sprintf(path, "/proc/%d/comm", pid);
			fd = fopen(path, "r");
			if (!fd)
				continue;
			len = fread(name, 1, sizeof(name), fd);
			fclose(fd);
			name[len] = '\0';

			if (strstr(name, home_daemon)) {
				LOG_INFO("%s is ready", home_daemon);
				ready = true;
				break;
			}
		}
	}
	closedir(dir);

	return ready;
}

static volatile bool rvc = false;
static volatile int block_check = 1;
static bool write_gpio_file(char * file_path, char * buf)
{
	int gpio_file;
	int count;
	gpio_file = open(file_path, O_WRONLY, S_IWUSR|S_IWGRP|S_IWOTH);
	if(gpio_file == -1) {
		LOG_ERR("open %s fail, return", file_path);
		close(gpio_file);
		return false;
	}
	count = write(gpio_file, buf, strlen(buf));
	if(count != strlen(buf)) {
		LOG_ERR("write %s to %s fail, return", buf, file_path);
		close(gpio_file);
		return false;
	}
	close(gpio_file);
	return true;
}
static void* rvc_detect_thread(void *data)
{
	FILE *fd = NULL;
	char file_path[256];
	char * tmp;
	int gpio_number;
	int gpio_level;
	int gpio_vlaue;
	char buf[5];
    int tm = 0;
    int pre = -1;
    int current = -1;

	prctl(PR_SET_NAME, "rvc_detect_thread");
	LOG_INFO("rvc_detect_thread start");
#if 1
	tmp = xml_get_setting(data, "gpio_number");
	if(tmp) {
		gpio_number = atoi(tmp);
		LOG_DBG("gpio_number is %d", gpio_number);
		sprintf(file_path, "/sys/class/gpio/gpio%d/value", gpio_number);
	} else {
		LOG_ERR("can't find gpio_number in config file, return");
		block_check = 0;
		return 0;
	}
#else
	// for debug and stress test
	sprintf(file_path, "/bin/rvc_flag");
#endif
	tmp = xml_get_setting(data, "gpio_level");
	if(tmp) {
		gpio_level = atoi(tmp);
		LOG_DBG("gpio_level is %d", gpio_level);
	} else {
		LOG_ERR("can't find gpio_level in config file, return");
		block_check = 0;
		return 0;
	}
	while (!fd) {
		fd = fopen(file_path, "r");
		if (!fd) {
			rvc = false;
			block_check = 0;
			usleep(50000);
		}
	}
	while (1) {
        rewind(fd);
        fflush(fd);
        fscanf(fd, "%d", &gpio_vlaue);
        current = gpio_vlaue;
        if (pre == current) {
            if (tm >= RVC_MAX_TIMES) {
                tm = 0;
                rvc = (current == gpio_level) ? true : false;
                block_check = 0;
            }
        } else {
            tm = 0;
        }
        pre = current;
        usleep(RVC_SLEEP_TIMES*1000);
        tm += RVC_SLEEP_TIMES;
	}
	fclose(fd);
	return 0;
}

static bool check_rvc()
{
	while (block_check);
	return rvc;
}

static void active_logo()
{
	MUTEX_LOCK(mutex_link);
	if (logo_link != NULL)
		link_active_modules(logo_link);
	//pause_logo_requested = false;
	MUTEX_UNLOCK(mutex_link);
}

static void stop_logo()
{
	MUTEX_LOCK(mutex_link);
	if (logo_link != NULL)
		link_stop_modules(logo_link);
	pause_logo_requested = true;
	MUTEX_UNLOCK(mutex_link);
}

static void* camera_thread(void *data)
{
	prctl(PR_SET_NAME, "camera_thread");
	LOG_INFO("camera_thread start");

	link_path *link;
	struct camera_thread_parameter * thread_data = (struct camera_thread_parameter *)data;

	link = xml_create_camera(thread_data->handle);
	if (link == NULL) {
		LOG_ERR("create link fail");
		return NULL;
	}

	MUTEX_LOCK(mutex_link);
	camera_link = link;
	MUTEX_UNLOCK(mutex_link);

	if (0 != link_init_modules(link)) {
		LOG_ERR("link_init_modules fail");
		return NULL;
	}

	static bool ui_camera_display   = false;
	while (1) {
		if (is_camera_display()) {
			//stop_logo();
			pause_logo();
			if (0 != link_active_modules(link)) {
				break;
			}
			while (is_camera_display()) {
                int display_mode = RvcDisplayMode_Unknown;
                if (getRvcMode() == RvcMode_NormalRvc) {
                    setDisplayMode(RvcDisplayMode_CameraOn);
                    display_mode = RvcDisplayMode_CameraOn;
                } else {
                    setDisplayMode(RvcDisplayMode_RvcCameraOn);
                    display_mode = RvcDisplayMode_RvcCameraOn;
                }
				usleep(1000);

                if (!ui_camera_display || getNotifyUi() == RvcDisplayMode_CameraOn) {
                    if (display_mode == RvcDisplayMode_CameraOn) {
                        notifyDisplayMode(RvcDisplayMode_CameraOn);
                    } else if (display_mode == RvcDisplayMode_RvcCameraOn) {
                        notifyDisplayMode(RvcDisplayMode_RvcCameraOn);
                    }
                    ui_camera_display = true;
                    setNotifyUi(RvcDisplayMode_Unknown);
				}
			}

			setDisplayMode(RvcDisplayMode_CameraOff);
			link_pause_modules(link);
			active_logo(); // we will not switch to logo any more

		} else if (thread_data->in_kernel_domain && is_home_ready(thread_data->handle)) {
			LOG_INFO("exit camera_thread");
			break;
		}

        if (ui_camera_display || getNotifyUi() == RvcDisplayMode_CameraOff) {
            notifyDisplayMode(RvcDisplayMode_CameraOff);
            ui_camera_display = false;
            setNotifyUi(RvcDisplayMode_Unknown);
        }

        if (getRvcMode() == RvcMode_EarlyRvc && getUiReadyStatus() == RvcUiReadyStatus_Yes) {
            setRvcMode(RvcMode_NormalRvc);
        }
		usleep(1000);
	}
	link_stop_modules(camera_link);

	return NULL;
}


static void* listener_thread(void *data) {
   prctl(PR_SET_NAME, "fastrvc_listener_thread");
   LOG_INFO("fastrvc_listener_thread start");

    startRvcListener();
    return NULL;
}


static bool is_camera_display() {
    bool ret = false;
    if (getRvcMode() == RvcMode_EarlyRvc) {
        ret = check_rvc();
    } else {
        int target_mode = getTargetDisplayMode();
        if (target_mode == RvcDisplayMode_CameraOn || target_mode == RvcDisplayMode_RvcCameraOn) {
            ret = true;
        } else {
            ret = false;
        }
    }

    return ret;
}

static int get_data(struct eMMC_data* info )
{
	int ret;
	int32_t signature = CONFIGS_DATA_SIGNATURE;
	uint32_t seek;
	switch(info->user){
			case 0: seek = CONFIGS_DATA_A_USER0_SEEK;break;
			case 10: seek = CONFIGS_DATA_A_USER1_SEEK;break;
			case 11: seek = CONFIGS_DATA_A_USER2_SEEK;break;
			case 12: seek = CONFIGS_DATA_A_USER3_SEEK;break;
	}
	struct data_info_item szinfo[INFO_INDEX_MAX];
	memset(szinfo, 0, DATA_INFO_LEN);
	if(read_data_info(szinfo , seek) == DATA_INFO_LEN) {
		if( (!memcmp(INFO_ITEM_NAME[INFO_INDEX_SIGA], szinfo[INFO_INDEX_SIGA].cName, 4))
			&& (signature == szinfo[INFO_INDEX_SIGA].ulValue)) {
			if( check_data(szinfo) ) {
				info->rbst = szinfo[INFO_INDEX_RBST].ulValue;
				info->ybst = szinfo[INFO_INDEX_YBST].ulValue;
				info->rcst = szinfo[INFO_INDEX_RCST].ulValue;
				info->ycst = szinfo[INFO_INDEX_YCST].ulValue;
				LOG_INFO("get emmc data success;rbst= %d ybst= %d rcst= %d ycst= %d " , info->rbst ,info->ybst ,info->rcst ,info->ycst);
				ret = RETURN_OK;
			} else {
				memset(szinfo, 0 ,DATA_INFO_LEN);
				seek = seek + 4*512;
				if( read_data_info(szinfo , seek) == DATA_INFO_LEN ) {
					if(!memcmp(INFO_ITEM_NAME[INFO_INDEX_SIGA], szinfo[INFO_INDEX_SIGA].cName, 4)
						&& CONFIGS_DATA_SIGNATURE == szinfo[INFO_INDEX_SIGA].ulValue) {
						if( check_data(szinfo) ) {
							info->rbst = szinfo[INFO_INDEX_RBST].ulValue;
							info->ybst = szinfo[INFO_INDEX_YBST].ulValue;
							info->rcst = szinfo[INFO_INDEX_RCST].ulValue;
							info->ycst = szinfo[INFO_INDEX_YCST].ulValue;
							return RETURN_OK;
						} else {
							ret = RETURN_READ_ERROR;
						}
					}
					else {
						ret = RETURN_SIGNATURE;
					}
				} else {
					ret = RETURN_READ_ERROR;
				}
			}
		} else {
			ret = RETURN_SIGNATURE;
		}
	} else { /*A read failed,read B*/
		memset(szinfo, 0, DATA_INFO_LEN);
		uint32_t seek;
		switch(info->user){
			case 0: seek = CONFIGS_DATA_B_USER0_SEEK;break;
			case 10: seek = CONFIGS_DATA_B_USER1_SEEK;break;
			case 11: seek = CONFIGS_DATA_B_USER2_SEEK;break;
			case 12: seek = CONFIGS_DATA_B_USER3_SEEK;break;
	}
		if(read_data_info(szinfo , seek) == DATA_INFO_LEN){
			if(!memcmp(INFO_ITEM_NAME[INFO_INDEX_SIGA], szinfo[INFO_INDEX_SIGA].cName, 4)
				&& CONFIGS_DATA_SIGNATURE == szinfo[INFO_INDEX_SIGA].ulValue) {
				if( check_data(szinfo) ) {
					info->rbst = szinfo[INFO_INDEX_RBST].ulValue;
					info->ybst = szinfo[INFO_INDEX_YBST].ulValue;
					info->rcst = szinfo[INFO_INDEX_RCST].ulValue;
					info->ycst = szinfo[INFO_INDEX_YCST].ulValue;

					return RETURN_OK;
				} else {
					ret = RETURN_READ_ERROR;
				}
			}
			else {
				ret = RETURN_SIGNATURE;
			}

		}else {
			ret = RETURN_READ_ERROR;
		}
	}
	return ret;
}

static int get_other(struct other_data* info )
{
	int ret;
	int32_t signature = CONFIGS_DATA_SIGNATURE;
	uint32_t seek = CONFIGS_DATA_A_CURRENT_SEEK;
	struct data_info_item szinfo[INFO_INDEX_ALL];
	memset(szinfo, 0, DATA_INFO_LEN);
	if(read_data_info(szinfo , seek) == DATA_INFO_LEN) {
		if( (!memcmp(INFO_ITEM_NAME[INFO_INDEX_TITA], szinfo[INFO_INDEX_TITA].cName, 4))
			&& (signature == szinfo[INFO_INDEX_TITA].ulValue)) {
			if( check_data_other(szinfo) ) {
				info->cusr = szinfo[INFO_INDEX_CUSR].ulValue;
				info->guid = szinfo[INFO_INDEX_GUID].ulValue;
				LOG_INFO("get other data success;cusr= %d guid= %d" , info->cusr ,info->guid);
				ret = RETURN_OK;
			} else {
				memset(szinfo, 0 ,DATA_INFO_LEN);
				seek = CONFIGS_DATA_B_CURRENT_SEEK;
				if( read_data_info(szinfo , seek) == DATA_INFO_LEN ) {
					if(!memcmp(INFO_ITEM_NAME[INFO_INDEX_TITA], szinfo[INFO_INDEX_TITA].cName, 4)
						&& CONFIGS_DATA_SIGNATURE == szinfo[INFO_INDEX_TITA].ulValue) {
						if( check_data_other(szinfo) ) {
							info->cusr = szinfo[INFO_INDEX_CUSR].ulValue;
							info->guid = szinfo[INFO_INDEX_GUID].ulValue;
							return RETURN_OK;
						} else {
							ret = RETURN_READ_ERROR;
						}
					}
					else {
						ret = RETURN_SIGNATURE;
					}
				} else {
					ret = RETURN_READ_ERROR;
				}
			}
		} else {
			ret = RETURN_SIGNATURE;
		}
	} else { /*A read failed,read B*/
		memset(szinfo, 0, DATA_INFO_LEN);
		uint32_t seek = CONFIGS_DATA_B_CURRENT_SEEK;
		if(read_data_info(szinfo , seek) == DATA_INFO_LEN){
			if(!memcmp(INFO_ITEM_NAME[INFO_INDEX_TITA], szinfo[INFO_INDEX_TITA].cName, 4)
				&& CONFIGS_DATA_SIGNATURE == szinfo[INFO_INDEX_TITA].ulValue) {
				if( check_data_other(szinfo) ) {
					info->cusr = szinfo[INFO_INDEX_CUSR].ulValue;
					info->guid = szinfo[INFO_INDEX_GUID].ulValue;
					return RETURN_OK;
				} else {
					ret = RETURN_READ_ERROR;
				}
			}
			else {
				ret = RETURN_SIGNATURE;
			}

		}else {
			ret = RETURN_READ_ERROR;
		}
	}
	return ret;
}

static bool check_data(struct data_info_item *pinfo)
{
	int32_t result = 0;
	for(int i= INFO_INDEX_RBST ; i < INFO_INDEX_MAX ; i++){
		result += pinfo[i].ulValue;
	}
	if(result == pinfo[INFO_INDEX_CRCC].ulValue)
		return true;
	else
		return false;
}


static bool check_data_other(struct data_info_item *pinfo)
{
	int32_t result = 0;
	for(int i= INFO_INDEX_CUSR ; i < INFO_INDEX_ALL ; i++){
		result += pinfo[i].ulValue;
	}
	if(result == pinfo[INFO_INDEX_CCRC].ulValue)
		return true;
	else
		return false;
}



static int read_data_info(struct data_info_item *pinfo , uint32_t seek)
{
	int ret;
	int fd = open(DATA_FILE_PATH , O_RDONLY);
	if(fd < 0) {
		LOG_ERR("BUInfoDriver Open Failed:%s" , strerror(errno));
		return -1;
	}

	if(lseek(fd, seek, SEEK_SET) == -1) {
		LOG_ERR("BUInfoDriver lseek Failed:%s" , strerror(errno));
		close(fd);
		return -1;
	}

	ret = read(fd , pinfo , DATA_INFO_LEN);
	if(ret < 0) {
		LOG_ERR("BUInfoDriver read Failed:%s" , strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);
	return ret;
}



/*
 * dayNightFlag =0 :day;dayNightFlag =1 :night
 * rgbOrCameraFlag =1 :rgb;rgbOrCameraFlag = 0:camera
*/
static int convertToBrightness(bool dayNightFlag,int brightness_step) {
	int brightness;
	if( dayNightFlag == true ){
		brightness = backlight_step[brightness_step + 8];//NIGHT
	} else {
		brightness = backlight_step[brightness_step + 25];//DAY
	}
	return brightness;
}

static int convertToContrast(bool rgbOrCameraFlag, int contrast_step) {
	int index = 0;
	if( rgbOrCameraFlag == false){
		index = contrast_step + 8;//CAMERA
	}else{
		index = contrast_step + 25;//ANDROID
	}
	return index;
}


static int write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;
    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        char buffer[20];
        size_t bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
        if(bytes >= sizeof(buffer)) return -EINVAL;
        ssize_t amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            already_warned = 1;
        }
        return -errno;
    }
}

int drm_res_init(int fd)
{
	drmModeResPtr res;

	res = drmModeGetResources(fd);
	if (!res) {
		LOG_ERR("Failed to get resources: %s.", strerror(errno));
		return -1;
	}

	{
		int i;
		drmModeCrtcPtr c;

		for (i = 0; i < res->count_crtcs; i++) {
			c = drmModeGetCrtc(fd, res->crtcs[i]);

			if (!c) {
				LOG_ERR("Could not get crtc %u: %s.",
				res->crtcs[i], strerror(errno));
				continue;
			}

			LOG_INFO("CRTC %u.", c->crtc_id);
			crtc_id = c->crtc_id;
			break;
			drmModeFreeCrtc(c);
		}
	}

	{
		unsigned int i;
		drmModeObjectPropertiesPtr props;

		props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);

		if (!props) {
			LOG_ERR("\tNo properties: %s.", strerror(errno));
			return -2;
		}

		for (i = 0; i < props->count_props; i++)
		{
			drmModePropertyPtr prop;
			prop = drmModeGetProperty(fd, props->props[i]);
			if (strncmp(prop->name, "PQPARAM", 7) == 0)
			{
				prop_id[0] = prop->prop_id;
				continue;
			}
			if (strncmp(prop->name, "PQINDEX", 7) == 0)
			{
				prop_id[1] = prop->prop_id;
				continue;
			}
			drmModeFreeProperty(prop);
		}
		drmModeFreeObjectProperties(props);
	}

	return 0;
}

int setContrast(int Contrast)
{
	LOG_INFO("setContrast is %d.",Contrast);
	DISP_PQ_PARAM pqparam;
	memcpy(&pqparam , &pqparam_standard ,sizeof(DISP_PQ_PARAM));
	pqparam.u4Contrast = Contrast;
	setParam(&pqparam);
	return 0;
}



int setParam(DISP_PQ_PARAM *pqParam)
{
	drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0)
    {
        LOG_ERR("drmOpen fail. ERR:%d/%s", errno, strerror(errno));
    }

	int ret;
    ret = drmModeObjectSetProperty(drm_fd, crtc_id, DRM_MODE_OBJECT_CRTC, prop_id[0], (unsigned long long)pqParam);
	LOG_INFO("ret = %d drm_fd=%d // crtc_id=%d // prop_id[0]=%d", ret, drm_fd, crtc_id, prop_id[0]);

    if(drm_fd >= 0)
		close(drm_fd);
    return 0;
}

int setIndex(const DISPLAY_PQ_T *pqIndex)
{
	drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0)
    {
        LOG_ERR("drmOpen fail. ERR:%d/%s", errno, strerror(errno));
    }
	drm_res_init(drm_fd);

	int ret;
    ret = drmModeObjectSetProperty(drm_fd, crtc_id, DRM_MODE_OBJECT_CRTC, prop_id[1], (unsigned long long)pqIndex);
	LOG_INFO("ret = %d drm_fd=%d // crtc_id=%d // prop_id[1]=%d", ret, drm_fd, crtc_id, prop_id[1]);

	if(drm_fd >= 0)
		close(drm_fd);

    return 0;
}

static void* recovery_backlight_thread(void *data)
{
	LOG_INFO("recovery_backlight_thread start.");
	struct eMMC_data backup;
	struct other_data other;
	int brightness;
	int contrast;
	bool display_mode = false;
	bool ILL_status = false;
	bool recovery_flag = true;
	int err = 0;
	int ret = 0;
	int count = 0;
	ret =  get_other(&other);
	if(ret != RETURN_OK) {
		LOG_ERR("recovery_backlight_thread get other data failed,use default data.:%s.", strerror(errno));
		memcpy(&other,&other_def,sizeof( other));
	}
	backup.user = other.cusr;
	err = get_data(&backup);
	if(err != RETURN_OK) {
		LOG_ERR("recovery_backlight_thread get data failed,use default data.:%s.", strerror(errno));
		memcpy(&backup,&user_def,sizeof( backup));
	}

	setIndex(&pqindex);

	while(true) {
		if(ILL_status != check_ILL() || display_mode != check_rvc() || recovery_flag) {
			display_mode  = check_rvc(); //1:camera; 0: android
			ILL_status = check_ILL(); // 1: NIGHT; 0: DAY

			if(ILL_status) {
				if(display_mode) {
					brightness = convertToBrightness(1 , backup.ybst);//night camera mode
				}else {
					brightness = convertToBrightness(1 ,backup.rbst);//night rgb mode
				}
			} else {
				if(display_mode) {
					brightness = convertToBrightness(0 , backup.ybst);//day camera mode
				}else {
					brightness = convertToBrightness(0 , backup.rbst);//day rgb mode
				}
			}

			pthread_mutex_lock(&g_lock);
			err = write_int(BRIGHTNESS_FILE_PATH, brightness);
		    pthread_mutex_unlock(&g_lock);
			if(err != 0) {
				LOG_ERR("recovery_backlight_thread write brightness file failed:%s.", strerror(errno));
			}

			if(display_mode) {
				contrast = convertToContrast(0 , backup.ycst); //camera mode
			} else {
				contrast = convertToContrast(1 , backup.rcst); //rgb mode
			}
			setContrast(contrast);
			recovery_flag = false;
		} else {
			usleep(5000);
			count ++;
		}
		if(count > 1200)
			break;
	}

	LOG_INFO("recovery_backlight_thread end.");

	return NULL;
}


static void* recovery_keybacklight_thread(void *data)
{
	LOG_INFO("recovery_keybacklight_thread start.");
	int keybrightness = 23;
	int err = 0;
	for(int i = 0; i < 25; i++) {
		keybrightness += 40;
		err =  write_int(KEYBRIGHTNESS_FILE_PATH, keybrightness);
		if(err != 0) {
			LOG_ERR("recovery_keybacklight_thread write brightness file failed:%s.", strerror(errno));
		}
		usleep(40000);
	}
	LOG_INFO("recovery_keybacklight_thread end.");
	return NULL;
}

static bool check_ILL()
{
	return false;
}
