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
#ifndef _FASTRVC_DEF_H
#define _FASTRVC_DEF_H
// Define of socket command ID
#define kSocketCmdDisplay             101
#define kSocketCmdUiReady             102
#define kSocketCmdReuqestDisplayMode  103
#define kSocketCmdUiReadyAck          104
#define kSocketCmdStopAnimatedLogo    105


enum RvcDisplayMode {
    RvcDisplayMode_Unknown = 0,
    RvcDisplayMode_CameraOff,
    RvcDisplayMode_CameraOn,
    RvcDisplayMode_RvcCameraOn,
};

enum RvcUiReadyStatus {
    RvcUiReadyStatus_Unknown = 0,
    RvcUiReadyStatus_No,
    RvcUiReadyStatus_Yes,
};

typedef enum {
    RvcMode_EarlyRvc = 0,
    RvcMode_NormalRvc,
} RvcMode;

#endif
