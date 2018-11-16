/*
 * Copyright (C) 2008-2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _RVCSOCKETLISTENER_H
#define _RVCSOCKETLISTENER_H

#include <pthread.h>

#include <sysutils/SocketClient.h>

class RvcSocketListener;
class RvcSocketEvent;

class RvcSocketListener {
    static const int kMsgBufferSize = 1024;

    bool                    mListen;
    const char              *mSocketName;
    int                     mSock;
    SocketClientCollection  *mClients;
    pthread_mutex_t         mClientsLock;
    int                     mCtrlPipe[2];
    pthread_t               mThread;
    bool                    mUseCmdNum;
    char                    mMsgTail[kMsgBufferSize];

public:
    RvcSocketListener(const char *socketName, bool listen);
    RvcSocketListener(const char *socketName, bool listen, bool useCmdNum);
    RvcSocketListener(int socketFd, bool listen);

    virtual ~RvcSocketListener();
    int startListener();
    int startListener(int backlog);
    int stopListener();

    void sendBroadcast(int code, const char *msg, bool addErrno);

    bool release(SocketClient *c) { return release(c, true); }

protected:
    bool onDataAvailable(SocketClient *c);
    void handleMessage(SocketClient *cli, RvcSocketEvent *evt);

private:
    bool release(SocketClient *c, bool wakeup);
    static void *threadStart(void *obj);
    void runListener();
    void init(const char *socketName, int socketFd, bool listen, bool useCmdNum);
};
#endif

