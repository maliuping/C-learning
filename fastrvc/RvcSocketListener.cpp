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

#define LOG_TAG "fastrvc"

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cutils/sockets.h>
#include <log/log.h>
#include <sysutils/SocketClient.h>

#include "RvcSocketListener.h"
#include "RvcSocketEvent.h"
#include "FastrvcDef.h"
#include "cpp_util.h"

#define CtrlPipe_Shutdown 0
#define CtrlPipe_Wakeup   1


RvcSocketListener::RvcSocketListener(const char *socketName, bool listen) {
    init(socketName, -1, listen, false);
}

RvcSocketListener::RvcSocketListener(int socketFd, bool listen) {
    init(NULL, socketFd, listen, false);
}

RvcSocketListener::RvcSocketListener(const char *socketName, bool listen, bool useCmdNum) {
    init(socketName, -1, listen, useCmdNum);
}

void RvcSocketListener::init(const char *socketName, int socketFd, bool listen, bool useCmdNum) {
    mListen = listen;
    mSocketName = socketName;
    mSock = socketFd;
    mUseCmdNum = useCmdNum;
    pthread_mutex_init(&mClientsLock, NULL);
    mClients = new SocketClientCollection();
}

RvcSocketListener::~RvcSocketListener() {
    if (mSocketName && mSock > -1)
        close(mSock);

    if (mCtrlPipe[0] != -1) {
        close(mCtrlPipe[0]);
        close(mCtrlPipe[1]);
    }
    SocketClientCollection::iterator it;
    for (it = mClients->begin(); it != mClients->end();) {
        (*it)->decRef();
        it = mClients->erase(it);
    }
    delete mClients;
}

int RvcSocketListener::startListener() {
    return startListener(4);
}

int RvcSocketListener::startListener(int backlog) {

    if (!mSocketName && mSock == -1) {
        SLOGE("Failed to start unbound listener");
        errno = EINVAL;
        return -1;
    } else if (mSocketName) {
        int socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socketFd < 0) {
            SLOGE("NHRvcSocketListener::startListener create socket '%s' failed: %s",
                mSocketName, strerror(errno));
            return -1;
        }

        if (true == mListen) {
            int optval = 1;
            TEMP_FAILURE_RETRY(setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)));

            unlink(mSocketName);

            struct sockaddr_un un;
            memset(&un, 0, sizeof(un));
            un.sun_family = AF_UNIX;
            strncpy(un.sun_path, mSocketName, sizeof(un.sun_path) - 1);
            int len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
            if (bind(socketFd, reinterpret_cast<struct sockaddr*>(&un), len) < 0) {
                SLOGE("NHRvcSocketListener::startListener bind socket '%s' failed: %s",
                    mSocketName, strerror(errno));
                unlink(mSocketName);
                close(socketFd);
                return -1;
            }

            if (lchown(un.sun_path, 0, 1000)) {
                SLOGE("Failed to lchown socket '%s'", un.sun_path);
                unlink(mSocketName);
                close(socketFd);

            }
            if (fchmodat(AT_FDCWD, un.sun_path, 0660, AT_SYMLINK_NOFOLLOW)) {
                SLOGE("Failed to fchmodat socket '%s'", un.sun_path);
                unlink(mSocketName);
                close(socketFd);

            }
        }

        mSock = socketFd;
        SLOGD("NHRvcSocketListener::startListener got mSock = %d for %s", mSock, mSocketName);

    }

    if (mListen && listen(mSock, backlog) < 0) {
        SLOGE("Unable to listen on socket (%s)", strerror(errno));
        return -1;
    } else if (!mListen)
        mClients->push_back(new SocketClient(mSock, false, mUseCmdNum));

    if (pipe(mCtrlPipe)) {
        SLOGE("pipe failed (%s)", strerror(errno));
        return -1;
    }

    if (pthread_create(&mThread, NULL, RvcSocketListener::threadStart, this)) {
        SLOGE("pthread_create (%s)", strerror(errno));
        return -1;
    }

    return 0;
}

int RvcSocketListener::stopListener() {
    char c = CtrlPipe_Shutdown;
    int  rc;

    rc = TEMP_FAILURE_RETRY(write(mCtrlPipe[1], &c, 1));
    if (rc != 1) {
        SLOGE("Error writing to control pipe (%s)", strerror(errno));
        return -1;
    }

    void *ret;
    if (pthread_join(mThread, &ret)) {
        SLOGE("Error joining to listener thread (%s)", strerror(errno));
        return -1;
    }
    close(mCtrlPipe[0]);
    close(mCtrlPipe[1]);
    mCtrlPipe[0] = -1;
    mCtrlPipe[1] = -1;

    if (mSocketName && mSock > -1) {
        close(mSock);
        mSock = -1;
    }

    SocketClientCollection::iterator it;
    for (it = mClients->begin(); it != mClients->end();) {
        delete (*it);
        it = mClients->erase(it);
    }
    return 0;
}

void *RvcSocketListener::threadStart(void *obj) {
    RvcSocketListener *me = reinterpret_cast<RvcSocketListener *>(obj);

    me->runListener();
    pthread_exit(NULL);
    return NULL;
}

void RvcSocketListener::runListener() {

    SocketClientCollection pendingList;

    while(1) {
        SocketClientCollection::iterator it;
        fd_set read_fds;
        int rc = 0;
        int max = -1;

        FD_ZERO(&read_fds);

        if (mListen) {
            max = mSock;
            FD_SET(mSock, &read_fds);
        }

        FD_SET(mCtrlPipe[0], &read_fds);
        if (mCtrlPipe[0] > max)
            max = mCtrlPipe[0];

        pthread_mutex_lock(&mClientsLock);
        for (it = mClients->begin(); it != mClients->end(); ++it) {
            // NB: calling out to an other object with mClientsLock held (safe)
            int fd = (*it)->getSocket();
            FD_SET(fd, &read_fds);
            if (fd > max) {
                max = fd;
            }
        }
        pthread_mutex_unlock(&mClientsLock);
        SLOGV("mListen=%d, max=%d, mSocketName=%s", mListen, max, mSocketName);
        if ((rc = select(max + 1, &read_fds, NULL, NULL, NULL)) < 0) {
            if (errno == EINTR)
                continue;
            SLOGE("select failed (%s) mListen=%d, max=%d", strerror(errno), mListen, max);
            sleep(1);
            continue;
        } else if (!rc)
            continue;

        if (FD_ISSET(mCtrlPipe[0], &read_fds)) {
            char c = CtrlPipe_Shutdown;
            TEMP_FAILURE_RETRY(read(mCtrlPipe[0], &c, 1));
            if (c == CtrlPipe_Shutdown) {
                break;
            }
            continue;
        }
        if (mListen && FD_ISSET(mSock, &read_fds)) {
            int c = TEMP_FAILURE_RETRY(accept4(mSock, nullptr, nullptr, SOCK_CLOEXEC));
            if (c < 0) {
                SLOGE("accept failed (%s)", strerror(errno));
                sleep(1);
                continue;
            }
            pthread_mutex_lock(&mClientsLock);
            mClients->push_back(new SocketClient(c, true, mUseCmdNum));
            pthread_mutex_unlock(&mClientsLock);
        }

        /* Add all active clients to the pending list first */
        pendingList.clear();
        pthread_mutex_lock(&mClientsLock);
        for (it = mClients->begin(); it != mClients->end(); ++it) {
            SocketClient* c = *it;
            // NB: calling out to an other object with mClientsLock held (safe)
            int fd = c->getSocket();
            if (FD_ISSET(fd, &read_fds)) {
                pendingList.push_back(c);
                c->incRef();
            }
        }
        pthread_mutex_unlock(&mClientsLock);

        /* Process the pending list, since it is owned by the thread,
         * there is no need to lock it */
        while (!pendingList.empty()) {
            /* Pop the first item from the list */
            it = pendingList.begin();
            SocketClient* c = *it;
            pendingList.erase(it);
            /* Process it, if false is returned, remove from list */
            if (!onDataAvailable(c)) {
                release(c, false);
            }
            c->decRef();
        }
    }
}

bool RvcSocketListener::release(SocketClient* c, bool wakeup) {
    bool ret = false;
    /* if our sockets are connection-based, remove and destroy it */
    if (mListen && c) {
        /* Remove the client from our array */
        SLOGV("going to zap %d for %s", c->getSocket(), mSocketName);
        pthread_mutex_lock(&mClientsLock);
        SocketClientCollection::iterator it;
        for (it = mClients->begin(); it != mClients->end(); ++it) {
            if (*it == c) {
                mClients->erase(it);
                ret = true;
                break;
            }
        }
        pthread_mutex_unlock(&mClientsLock);
        if (ret) {
            ret = c->decRef();
            if (wakeup) {
                char b = CtrlPipe_Wakeup;
                TEMP_FAILURE_RETRY(write(mCtrlPipe[1], &b, 1));
            }
        }
    }
    return ret;
}

void RvcSocketListener::sendBroadcast(int code, const char *msg, bool addErrno) {
    SocketClientCollection safeList;

    /* Add all active clients to the safe list first */
    safeList.clear();
    pthread_mutex_lock(&mClientsLock);
    SocketClientCollection::iterator i;

    for (i = mClients->begin(); i != mClients->end(); ++i) {
        SocketClient* c = *i;
        c->incRef();
        safeList.push_back(c);
    }
    pthread_mutex_unlock(&mClientsLock);

    while (!safeList.empty()) {
        /* Pop the first item from the list */
        i = safeList.begin();
        SocketClient* c = *i;
        safeList.erase(i);
        // broadcasts are unsolicited and should not include a cmd number
        if (c->sendMsg(code, msg, addErrno, false)) {
            SLOGW("Error sending broadcast (%s)", strerror(errno));
        }
        c->decRef();
    }
}

bool RvcSocketListener::onDataAvailable(SocketClient *c) {
    char buffer[kMsgBufferSize];
    int size;

    size = TEMP_FAILURE_RETRY(read(c->getSocket(), buffer, sizeof(buffer)));
    if (size < 0) {
        SLOGE("read() failed (%s)", strerror(errno));
        return false;
    } else if (!size) {
        return false;
    }

    bool haveTail = false;
    if (0 != strlen(mMsgTail)) {
        haveTail = true;
    }

    int strBegin = 0;
    int msgCount = 0;
    for (int i = 0; i < size; ++i) {
        if ('\n' == buffer[i]) {
            buffer[i] = '\0';
            char *message = buffer + strBegin;
            uint oldBegin = strBegin;
            strBegin = i + 1;

            // If no message, continue
            if (('\0' == message[0]) && !haveTail) {
                continue;
            }

            msgCount++;

            char* wholeMsg = NULL;  // A whole message including mMsgTail and the head part of buffer
            if (haveTail) {
                uint len = strlen(mMsgTail) + size - oldBegin + 1;
                wholeMsg = new char[len];
                if (NULL != wholeMsg) {
                    memset(wholeMsg, 0, len);
                    memcpy(wholeMsg, mMsgTail, strlen(mMsgTail));
                    memcpy(wholeMsg + strlen(mMsgTail), message, size - oldBegin);
                    message = wholeMsg;
                }
                else {
                    SLOGE(">>>>>onDataAvailable new failed...");
                }
            }

            // debug output
            SLOGE("onDataAvailable[%d] = %s", msgCount, message);

            RvcSocketEvent evt;
            if (evt.decode(message)) {
                handleMessage(c, &evt);
            }

            if (haveTail) {
                haveTail = false;
                memset(mMsgTail, 0, sizeof(mMsgTail));
            }
            if (NULL != wholeMsg) {
                delete[] wholeMsg;
                wholeMsg = NULL;
            }
        }
    }

    if (strBegin < size) {
        if (0 != strlen(mMsgTail)) {
            SLOGD(">>>>>onDataAvailable  this msg may be too long(>= 1024 Bytes) or be wrong. not supported.");
        }

        memset(mMsgTail, 0, sizeof(mMsgTail));
        memcpy(mMsgTail, buffer + strBegin, size - strBegin);
        SLOGD("*****onDataAvailable  msg tail (from %d): %s", strBegin, mMsgTail);
    }

    return true;
}

void RvcSocketListener::handleMessage(SocketClient *cli, RvcSocketEvent *evt) {
    if (!cli || !evt) {
        return;
    }

    int id = evt->getEventId();
    int value = evt->getValue();
    switch (id) {
    case kSocketCmdDisplay:
        {
            // display
            setTargetDisplayMode(value);
            if (value == RvcDisplayMode_RvcCameraOn || value == RvcDisplayMode_CameraOn) {
                setNotifyUi(RvcDisplayMode_CameraOn);
            } else {
                setNotifyUi(RvcDisplayMode_CameraOff);
            }
        }
        break;
    case kSocketCmdUiReady:
        {
            // set UI ready
            setUiReadyStatus(value);

            if(getDisplayMode() == RvcDisplayMode_RvcCameraOn) {
                notifyUiReadyAck(RvcDisplayMode_RvcCameraOn);
            } else if(getDisplayMode() == RvcDisplayMode_CameraOff) {
                notifyUiReadyAck(RvcDisplayMode_CameraOff);
            }
        }
        break;
    case kSocketCmdReuqestDisplayMode:
        {
            requestDisplayMode();
        }
        break;
    case kSocketCmdStopAnimatedLogo:
        {
            stopAnimatedLogo();
        }
        break;
    default:
        return;
    }
}
