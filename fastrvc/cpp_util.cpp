/**
 * Copyright @ 2018 Suntec Software(Shanghai) Co., Ltd.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are NOT permitted except as agreed by
 * Suntec Software(Shanghai) Co., Ltd.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include <unistd.h>
#include <cutils/sockets.h>

#include "cpp_util.h"
#include "RvcSocketListener.h"
#include "FastrvcDef.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "osal.h"

#define RVC_SOCKET_NAME "/dev/socket/rvc"

static const int kRvcMsgBuffer = 512;

static volatile int gTargetRvcDisplayMode   = RvcDisplayMode_CameraOff;
static volatile int gRvcUiReadyStatus = RvcUiReadyStatus_No;
static volatile int gRvcDisplayMode   = RvcDisplayMode_Unknown;
static volatile int gRvcMode = RvcMode_EarlyRvc;
static volatile int gStopAnimatedLogo = 0;
static volatile int gNotifyUi = RvcDisplayMode_Unknown;

static RvcSocketListener *gListener   = NULL;
static MUTEX_HANDLE(mutex_ui_ready);
static MUTEX_HANDLE(mutex_display_mode);
static MUTEX_HANDLE(mutex_rvc_mode);
static MUTEX_HANDLE(mutex_target_display_mode);
static MUTEX_HANDLE(mutex_stop_animated_logo);

void initCppUtil() {
    MUTEX_INIT(mutex_ui_ready);
    MUTEX_INIT(mutex_display_mode);
    MUTEX_INIT(mutex_rvc_mode);
    MUTEX_INIT(mutex_target_display_mode);
    MUTEX_INIT(mutex_stop_animated_logo);
}

int startRvcListener() {
    LOG_DBG("startRvcListener start");

    while (1) {
        if (!access("/dev/socket", F_OK)) {
            break;
        }
        usleep(100 * 1000);
    }

    if (!gListener) {
        gListener = new RvcSocketListener(RVC_SOCKET_NAME, 1);
    }
    if (!gListener) {
        LOG_ERR("Unable to create RvcSocketListener");
        exit(-1);
    }

    if (gListener->startListener()) {
        LOG_ERR("Unable to start RvcSocketListener");
        exit(-1);
    }

    LOG_DBG("startRvcListener OK");
    return 0;
}

void setUiReadyStatus(int status) {
    LOG_INFO("setUiReadyStatus[%d]", status);
    MUTEX_LOCK(mutex_ui_ready);
    gRvcUiReadyStatus = status;
    MUTEX_UNLOCK(mutex_ui_ready);
}

int getUiReadyStatus() {
    MUTEX_LOCK(mutex_ui_ready);
    int ui_ready = gRvcUiReadyStatus;
    MUTEX_UNLOCK(mutex_ui_ready);
    return ui_ready;
}

void setDisplayMode(int mode) {
    MUTEX_LOCK(mutex_display_mode);
    gRvcDisplayMode = mode;
    MUTEX_UNLOCK(mutex_display_mode);
}

int getDisplayMode() {
    MUTEX_LOCK(mutex_display_mode);
    int display_mode = gRvcDisplayMode;
    MUTEX_UNLOCK(mutex_display_mode);
    return display_mode;
}

void setTargetDisplayMode(int mode) {
    LOG_INFO("setTargetDisplayMode[%d]", mode);
    MUTEX_LOCK(mutex_target_display_mode);
    gTargetRvcDisplayMode = mode;
    MUTEX_UNLOCK(mutex_target_display_mode);
}

int getTargetDisplayMode() {
    MUTEX_LOCK(mutex_target_display_mode);
    int target_display_mode = gTargetRvcDisplayMode;
    MUTEX_UNLOCK(mutex_target_display_mode);
    return target_display_mode;
}

void setRvcMode(int mode) {
    MUTEX_LOCK(mutex_rvc_mode);
    gRvcMode = mode;
    MUTEX_UNLOCK(mutex_rvc_mode);
}

int getRvcMode() {
    MUTEX_LOCK(mutex_rvc_mode);
    int rvc_mode = gRvcMode;
    MUTEX_UNLOCK(mutex_rvc_mode);
    return rvc_mode;
}


void notifyDisplayMode(int mode) {
    if (gListener) {
        LOG_INFO("notifyDisplayMode[%d]", mode);
        char msg[kRvcMsgBuffer] = { 0 };
        snprintf(msg, sizeof(msg), "value=%d\n", mode);
        gListener->sendBroadcast(kSocketCmdDisplay, msg, false);
    }
}

void requestDisplayMode() {
    int mode = getDisplayMode();
    notifyDisplayMode(mode);
}


void notifyUiReadyAck(int mode) {
    if (gListener) {
        LOG_INFO("notifyUiReadyAck[%d]", mode);
        char msg[kRvcMsgBuffer] = { 0 };
        snprintf(msg, sizeof(msg), "value=%d\n", mode);
        gListener->sendBroadcast(kSocketCmdUiReadyAck, msg, false);
    }
}

void stopAnimatedLogo() {
    LOG_INFO("stopAnimatedLogo");
    MUTEX_LOCK(mutex_stop_animated_logo);
    gStopAnimatedLogo = 1;
    MUTEX_UNLOCK(mutex_stop_animated_logo);
}

int isStopAnimatedLogo() {
    MUTEX_LOCK(mutex_stop_animated_logo);
    bool ret = gStopAnimatedLogo;
    MUTEX_UNLOCK(mutex_stop_animated_logo);
    return ret;
}

void setNotifyUi(int is_notify_ui) {
    MUTEX_LOCK(mutex_target_display_mode);
    gNotifyUi = is_notify_ui;
    MUTEX_UNLOCK(mutex_target_display_mode);
}

int getNotifyUi() {
    MUTEX_LOCK(mutex_target_display_mode);
    int ret = gNotifyUi;
    MUTEX_UNLOCK(mutex_target_display_mode);
    return ret;
}

#ifdef __cplusplus
}
#endif
