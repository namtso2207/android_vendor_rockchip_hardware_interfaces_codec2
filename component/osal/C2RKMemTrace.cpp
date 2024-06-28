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

#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "C2RKMemTrace"

#include <string.h>
#include <sys/syscall.h>
#include <cutils/properties.h>

#include "C2RKLog.h"
#include "C2RKMemTrace.h"
#include "C2RKChipCapDef.h"

namespace android {

// TODO: do more restriction on total soc power load?
#define MAX_DEC_SOC_CAP_LOAD       (7680*4320*60)
#define MAX_ENC_SOC_CAP_LOAD       (7680*4320*30)


C2RKMemTrace::C2RKMemTrace() {
    mCurDecLoad = 0;
    mCurEncLoad = 0;
    mMaxInstanceNum = 32;

    if (C2RKChipCapDef::get()->getChipType() == RK_CHIP_3326) {
        mMaxInstanceNum = 16;
    }

    mDisableCheck = property_get_int32("codec2_disable_load_check", 0);
    if (mDisableCheck) {
        c2_info("property match, disable codec loading check.");
    }
}

bool C2RKMemTrace::tryAddVideoNode(C2NodeInfo &node) {
    Mutex::Autolock autoLock(mLock);

    if (node.client == nullptr) {
        c2_err("can't record node without client id.");
        return false;
    }

    if (hasNodeIteam(node.client)) {
        c2_info("ignore duplicate node, client %p", node.client);
        return true;
    }

    std::string sMime;
    int32_t load = 0;

    if (node.pid == 0) {
        node.pid = syscall(SYS_gettid);
    }

    if (node.frameRate <= 1.0f) {
        node.frameRate = 30.0f;
    }

    load = node.width * node.height * node.frameRate;

    if (node.type == C2_TRACE_DECODER) {
        if (mDisableCheck || ((mCurDecLoad + load < MAX_DEC_SOC_CAP_LOAD)
                && (mDecNodes.size() < mMaxInstanceNum))) {
            mDecNodes.push(node);
            mCurDecLoad += load;
            return true;
        }
        c2_err("overload initialize decoder(%dx%d@%.1f), current load %d",
                node.width, node.height, node.frameRate, mCurDecLoad);
    } else if (node.type == C2_TRACE_ENCODER) {
        if (mDisableCheck || ((mCurEncLoad + load < MAX_ENC_SOC_CAP_LOAD)
                && (mEncNodes.size() < mMaxInstanceNum))) {
            mEncNodes.push(node);
            mCurEncLoad += load;
            return true;
        }
        c2_err("overload initialize encoder(%dx%d@%.1f), current load %d",
                node.width, node.height, node.frameRate, mCurEncLoad);
    }

    return false;
}

void C2RKMemTrace::removeVideoNode(void *client) {
    Mutex::Autolock autoLock(mLock);

    size_t i = 0;
    for (i = 0; i < mDecNodes.size(); i++) {
        C2NodeInfo node = mDecNodes.editItemAt(i);
        if (node.client == client) {
            mCurDecLoad -= (node.width * node.height * node.frameRate);
            mDecNodes.removeAt(i);
            return;
        }
    }

    for (i = 0; i < mEncNodes.size(); i++) {
        C2NodeInfo node = mEncNodes.editItemAt(i);
        if (node.client == client) {
            mCurEncLoad -= (node.width * node.height * node.frameRate);
            mEncNodes.removeAt(i);
            return;
        }
    }
}

bool C2RKMemTrace::hasNodeIteam(void *client) {
    size_t i = 0;
    for (i = 0; i < mDecNodes.size(); i++) {
        C2NodeInfo node = mDecNodes.editItemAt(i);
        if (node.client == client) {
            return true;
        }
    }

    for (i = 0; i < mEncNodes.size(); i++) {
        C2NodeInfo node = mEncNodes.editItemAt(i);
        if (node.client == client) {
            return true;
        }
    }
    return false;
}

void C2RKMemTrace::dumpAllNode() {
    Mutex::Autolock autoLock(mLock);

    c2_info("======= Hardware Codec2 Memory Summary =======");
    c2_info("Total: %d dec nodes / %d enc nodes",
            mDecNodes.size(), mEncNodes.size());

    for (size_t i = 0; i < mDecNodes.size(); i++) {
        const C2NodeInfo &node = mDecNodes.editItemAt(i);
        c2_info("Decoder: ");
        c2_info("    Client: %p", node.client);
        c2_info("    Pid   : %d", node.pid);
        c2_info("    Mime  : %s", node.mime);
        c2_info("    Name  : %s", node.name);
        c2_info("    Size  : %dx%d", node.width, node.height);
        c2_info("    FrameRate: %.1f", node.frameRate);
    }

    for (size_t i = 0; i < mEncNodes.size(); i++) {
        const C2NodeInfo &node = mEncNodes.editItemAt(i);
        c2_info("Encoder: ");
        c2_info("    Client: %p", node.client);
        c2_info("    Pid   : %d", node.pid);
        c2_info("    Mime  : %s", node.mime);
        c2_info("    Name  : %s", node.name);
        c2_info("    Size  : %dx%d", node.width, node.height);
        c2_info("    FrameRate: %.1f", node.frameRate);
    }
    c2_info("===============================================");
}

}
