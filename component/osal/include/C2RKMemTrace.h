/*
 * Copyright 2023 Rockchip Electronics Co. LTD
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

#ifndef ANDROID_C2_RK_MEM_TRACE_H__
#define ANDROID_C2_RK_MEM_TRACE_H__

#include <utils/Mutex.h>
#include <utils/Vector.h>

namespace android {

class C2RKMemTrace {
public:
    static C2RKMemTrace* get() {
        static C2RKMemTrace _gInstance;
        return &_gInstance;
    }

    typedef enum {
        C2_TRACE_OTHER = 0,
        C2_TRACE_DECODER,
        C2_TRACE_ENCODER,
    } C2TraceType;

    typedef struct {
        void*       client;
        uint32_t    pid;
        const char *name;
        const char *mime;
        C2TraceType type;
        uint32_t    width;
        uint32_t    height;
        float       frameRate;
    } C2NodeInfo;

    bool tryAddVideoNode(C2NodeInfo &node);
    void removeVideoNode(void *client);
    void dumpAllNode();

private:
    C2RKMemTrace();
    virtual ~C2RKMemTrace() {};

    int32_t             mDisableCheck;
    int32_t             mCurDecLoad;
    int32_t             mCurEncLoad;
    uint32_t            mMaxInstanceNum;
    Mutex               mLock;
    Vector<C2NodeInfo>  mDecNodes;
    Vector<C2NodeInfo>  mEncNodes;

    bool hasNodeIteam(void *client);
};

}

#endif // ANDROID_C2_RK_MEM_TRACE_H__

