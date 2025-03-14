/**
 *
 *  Copyright 2025 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 **/

#ifndef NFC_VND_EXTENSION_API_H
#define NFC_VND_EXTENSION_API_H

#include <NfcExtension.h>
#include <string>

/** \addtogroup NFC_VND_EXTENSION_API_H
 *  @brief  interface to perform the NXP NFC functionality.
 *  @{
 */

#if (defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64))
  const std::string NXP_PROP_LIB_PATH = "/system/vendor/lib64/libnfc_prop_extn.so";
#else
  const std::string NXP_PROP_LIB_PATH = "/system/vendor/lib/libnfc_prop_extn.so";
#endif

constexpr uint8_t HAL_NFC_ERROR_EVT = 0x06;

#endif // NFC_VND_EXTENSION_API_H
