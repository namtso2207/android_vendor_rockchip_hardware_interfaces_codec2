/*
 * Copyright (C) 2023 Rockchip Electronics Co. LTD
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

#ifndef _ANDROID_C2_RK_VERSION_H_
#define _ANDROID_C2_RK_VERSION_H_

#define C2_VERSION_STR_HELPER(x) #x
#define C2_VERSION_STR(x) C2_VERSION_STR_HELPER(x)

/* Codec2 Component Verison */
#define C2_MAJOR_VERSION       1
#define C2_MINOR_VERSION       14
#define C2_REVISION_VERSION    3
#define C2_BUILD_VERSION       2

#define C2_COMPONENT_FULL_VERSION \
    C2_VERSION_STR(C2_MAJOR_VERSION) "." \
    C2_VERSION_STR(C2_MINOR_VERSION) "." \
    C2_VERSION_STR(C2_REVISION_VERSION) "_[" \
    C2_VERSION_STR(C2_BUILD_VERSION) "]"

#endif /* _ANDROID_C2_RK_VERSION_H_ */

