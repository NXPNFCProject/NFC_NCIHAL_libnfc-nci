/**
 *
 *  Copyright 2024 NXP
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

#ifndef NFC_EXTENSION_API_H
#define NFC_EXTENSION_API_H

#include "NfcExtensionConstants.h"
#include <NfcExtension.h>
#include <cstdint>
/** \addtogroup NFC_EXTENSION_API_INTERFACE
 *  @brief  interface to perform the NXP NFC functionality.
 *  @{
 */
extern "C" {

/**
 * @brief Init the extension library with stack and data callbacks.
 * @return void
 *
 */
void phNxpExtn_LibInit();

/**
 * @brief De init the extension library
 *        and resets all it's states
 * @return void
 *
 */
void phNxpExtn_LibDeInit();

/**
 * @brief Handles the nfc event with extenstion feature support
 * @param eventCode event code indicating the functionality
 * @param eventData data of the functionality
 * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
 * feature and it have to be handled by extension library. returns
 * NFCSTATUS_EXTN_FEATURE_FAILURE, if it have to be handled by NFC HAL.
 *
 */
NFCSTATUS phNxpExtn_HandleNfcEvent(NfcExtEvent_t eventCode,
                                   NfcExtEventData_t eventData);
}
/** @}*/
#endif // NFC_EXTENSION_API_H