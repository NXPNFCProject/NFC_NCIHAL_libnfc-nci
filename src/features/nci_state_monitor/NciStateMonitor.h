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

#ifndef NCI_STATE_MONITOR_H
#define NCI_STATE_MONITOR_H

#include "IEventHandler.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
// #include "PropExtensionConstants.h"
// #include "PropExtensionController.h"
#include <cstdint>
#include <map>
#include <vector>

constexpr uint8_t HAL_FATAL_ERR_CODE = 0xF2;
constexpr uint8_t HAL_TRANS_ERR_CODE = 0xF8;
constexpr uint8_t NCI_MODE_SET_CMD_EE_INDEX = 3;

/** \addtogroup STATEMONITOR_EVENT_HANDLER_API_INTERFACE
 *  @brief  interface to perform the NciStateMonitor feature functionality.
 *  @{
 */
class NciStateMonitor : public IEventHandler {
public:
  static NciStateMonitor *getInstance();
  /**
   * @brief handles the vendor NCI message by delegating the message to
   *        current active event handler
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciMessage(uint16_t dataLen, const uint8_t *pData);

  /**
   * @brief handles the vendor NCI response/Notification by delegating the
   * message to current active event handler
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData);

  /**
   * @brief on receiving NFCEE_MODE_SET_CMD for ESE, setting eseModeSetCmd to
   * true.
   *
   * @param dataLen
   * @param pData
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processExtnWrite(uint16_t *dataLen, uint8_t *pData) override;

private:
  static NciStateMonitor *instance;

  NciStateMonitor();
  ~NciStateMonitor();
};
/** @}*/
#endif // NCI_STATE_MONITOR_H
