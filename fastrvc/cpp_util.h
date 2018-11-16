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

#ifndef _CPP_UTIL_H
#define _CPP_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

void initCppUtil();
int startRvcListener();
void setUiReadyStatus(int status);
int getUiReadyStatus();
void setDisplayMode(int mode);
int getDisplayMode();
void notifyDisplayMode(int mode);
void requestDisplayMode();
void notifyUiReadyAck(int mode);
void setRvcMode(int mode);
int getRvcMode();
void setTargetDisplayMode(int mode);
int getTargetDisplayMode();
void stopAnimatedLogo();
int isStopAnimatedLogo();
void setNotifyUi(int is_notify_ui);
int getNotifyUi();


#ifdef __cplusplus
}
#endif

#endif
