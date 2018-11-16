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
 * @file RvcSocketEvent.h
 * @brief Declaration file of class RvcSocketEvent.
 */

#ifndef RVCSOCKETEVENT_H
#define RVCSOCKETEVENT_H

#ifndef __cplusplus
#    error ERROR: This file requires C++ compilation (use a .cpp suffix)
#endif


#define RVC_PARAMS_MAX 32

    class RvcSocketEvent;

    class RvcSocketEvent {
    public:
        RvcSocketEvent();
        virtual ~RvcSocketEvent();

        bool decode(const char *buffer);
        const char *findParam(const char *paramName);
        int getEventId() { return m_id; };
        int getValue()  { return m_value; };

    private:
        int m_id;
        int m_value;
        char *m_params[RVC_PARAMS_MAX];

        RvcSocketEvent(const RvcSocketEvent& ref);
        RvcSocketEvent & operator = (const RvcSocketEvent& ref);
    };
#endif // RVCSOCKETEVENT_H
/* EOF */
