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
/**
 * @file RvcSocketEvent.cpp
 * @brief Implementation file of class RvcSocketEvent.
 */
#include <stdlib.h>
#include <cutils/log.h>

#include "RvcSocketEvent.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "fastrvcmsg"

/* If the string 'str' has 'sub' characters
 * then return 'str + strlen(sub)', otherwise return
 * NULL.
 */
static const char*
getPrefixValue(const char* str, const char* sub)
{
    const char *needle = strstr(str, sub);
    if (!needle) {
        return NULL;
    }
    return needle + strlen(sub);
}

RvcSocketEvent::RvcSocketEvent()
{
    m_id    = -1;
    m_value = -1;
    memset(m_params, 0, sizeof(m_params));
}

RvcSocketEvent::~RvcSocketEvent()
{
    for (auto &param : m_params) {
        if(param) {
            free(param);
        }
    }
}

bool RvcSocketEvent::decode(const char *buffer)
{
    if (!buffer) {
        return false;
    }

    const char *str = buffer;
    m_id = atoi(str);
    const char *value = getPrefixValue(str, "value=");

    if (value) {
        m_value = atoi(value);
    }

    ALOGD("RvcSocketEvent[%s]decode id[%d] value[%d]", buffer, m_id, m_value);
    return true;
}



