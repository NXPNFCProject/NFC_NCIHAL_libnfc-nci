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
#ifndef QTAG_HANDLER_H
#define QTAG_HANDLER_H

#include "IEventHandler.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include <cstdint>
#include <vector>

constexpr uint8_t QTAG_NCI_VENDOR_RSP_SUCCESS[] = {0x4F, 0x3E, 0x01, 0x00};
constexpr uint8_t QTAG_NCI_VENDOR_RSP_FAILURE[] = {0x4F, 0x3E, 0x01, 0x01};

/** \addtogroup QTAG_EVENT_HANDLER_API_INTERFACE
 *  @brief  interface to perform the QTag feature functionality.
 *  @{
 */
class QTagHandler : public IEventHandler {
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
  NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen, const uint8_t *pData);

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

  /**
   * @brief on receiving RF_DISC_CMD, if QTag is enabled then update
   * RF_DISC_CMD with Q_POLL(0x71)
   *
   * @param dataLen
   * @param pData
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  NFCSTATUS processExtnWrite(uint16_t *dataLen, uint8_t *pData) override;

  QTagHandler();

private:
  std::vector<uint8_t> mQtagNciPkt;
};
/** @}*/
#endif // QTAG_HANDLER_H
