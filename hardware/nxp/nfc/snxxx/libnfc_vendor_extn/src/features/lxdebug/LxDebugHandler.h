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
#ifndef LxDebug_HANDLER_H
#define LxDebug_HANDLER_H
#include "IEventHandler.h"
#include "LxDebug.h"
#include <cstdint>
#include <vector>

/** \addtogroup LxDebug_EVENT_HANDLER_API_INTERFACE
 *  @brief  interface to perform the LxDebug feature functionality.
 *  @{
 */
class LxDebugHandler : public IEventHandler {
public:
  /**
   * @brief handles the vendor NCI message
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciMessage(uint16_t dataLen, const uint8_t *pData);
  /**
   * @brief handles the vendor NCI response and notification
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData);
  /**
   * @brief on Feature start, Current Handler have to be registered with
   * controller through switchEventHandler interface
   * @return void
   *
   */
  void onFeatureStart() override;
  /**
   * @brief on Feature End, Default Handler have to be registered with
   * controller through switchEventHandler interface
   * @return void
   *
   */
  void onFeatureEnd() override;
  LxDebugHandler();
  ~LxDebugHandler();
  NFCSTATUS processExtnWrite(uint16_t *dataLen, uint8_t *pData) override;

private:
  LxDebug *mLxDebugMngr = nullptr;
  static constexpr uint8_t LXDEBUG_SUB_GID = 0x07;
  static constexpr uint8_t FIELD_DETECT_MODE_SET_SUB_OID = 0x0F;
  static constexpr uint8_t IS_FIELD_DETECT_ENABLED_SUB_OID = 0x0E;
  static constexpr uint8_t IS_FIELD_DETECT_MODE_STARTED_SUB_OID = 0x0D;
};
/** @}*/
#endif // LxDebug_HANDLER_H
