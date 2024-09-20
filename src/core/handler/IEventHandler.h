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

#ifndef IEVENTHANDLER_GEN_H
#define IEVENTHANDLER_GEN_H

#include "NfcExtension.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include <cstdint>

/** \addtogroup EVENT_HANDLER_API_INTERFACE
 *  @brief  Base event handler class to handle the NCI message, response and
 * notification
 *  @{
 */
class IEventHandler {
public:
  /**
   * @brief handles the vendor NCI message
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  // TODO: Create LLD capturing the sequence of adding the new feature.
  virtual NFCSTATUS handleVendorNciMessage(uint16_t dataLen,
                                           const uint8_t *pData) = 0;

  /**
   * @brief handles the vendor NCI response and notification.
   *        responsible for stopping the response timer, if the vendor
   *        specific command is received
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   *
   */
  virtual NFCSTATUS handleVendorNciRspNtf(uint16_t dataLen,
                                          const uint8_t *pData) = 0;

  /**
   * @brief this callback will be invoked by controller when it
   *        successfully stopped the previous event handler
   * @return void
   *
   */
  virtual void onFeatureStart() {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s IEventHandler Enter",
                   __func__);
  }

  /**
   * @brief this callback will be invoked by controller when it
   *        wants to process the callback events to different
   *        event handler. This will be invoked, when vendor proprietary
   *        command is received from upper layer to process.
   * @return void
   *
   */
  virtual void onFeatureEnd() {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s IEventHandler Enter",
                   __func__);
  }

  /**
   * @brief this callback will be invoked by controller when HAL
   *        updates the error or failure. Respection feature
   *        Handler have to clean up or handle this error gracefully
   *
   * @return void
   *
   */
  virtual void handleHalEvent(int errorCode) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "IEventHandler::%s Enter errorCode:%d", __func__, errorCode);
  }

  /**
   * @brief handles the control granted callback, if concrete
   *        handler does not handle it
   * \note This function should be light weight and shall not
   * take much time to execute othewise all other processing
   * will be blocked.
   * @return returns void
   *
   */
  virtual void onWriteComplete(uint8_t status) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "IEventHandler::%s Enter status:%d", __func__, status);
  }

  /**
   * @brief handles the control granted callback, if concrete
   *        handler does not handle it
   * @return returns void
   *
   */
  virtual void halControlGranted() {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s IEventHandler Enter",
                   __func__);
  }
  /**
   * @brief indicates that HAL Exclusive request to LibNfc
   *        timed out
   * @return void
   *
   */
  virtual void onHalRequestControlTimeout() {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s onHalRequestControlTimeout Enter", __func__);
  }
  /**
   * @brief indicates that write response time out for the
   *        NCI packet sent to controller
   *
   * @return void
   *
   */
  virtual void onWriteRspTimeout() {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "IEventHandler::%s Enter",
                   __func__);
  }

  /**
   * @brief Send notification to upper layer about Generic error
   *
   * @return void
   *
   */
  void notifyGenericErrEvt(uint8_t ntfType) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "Generic Error 0x%X", ntfType);
    uint8_t rdrModeNtf[4] = {0x00};
    rdrModeNtf[0] = NCI_PROP_NTF_VAL;
    rdrModeNtf[1] = NCI_PROP_OID_VAL;
    rdrModeNtf[2] = 0x01; // NCI Length filed
    rdrModeNtf[3] = ntfType;
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        sizeof(rdrModeNtf), rdrModeNtf);
  }

  /**
   * @brief update RF_DISC_CMD, if specific poll/listen feature is enabled
   * with required poll or listen technologies & send to upper layer.
   * @param dataLen length RF_DISC_CMD
   * @param pData
   * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
   * feature and handled by extension library otherwise
   * NFCSTATUS_EXTN_FEATURE_FAILURE.
   */
  virtual NFCSTATUS processExtnWrite(uint16_t *dataLen, uint8_t *pData) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "IEventHandler::%s Enter",
                   __func__);
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }

  virtual ~IEventHandler() = default; // virtual destructor
};
/** @}*/
#endif // IEVENTHANDLER_GEN_H
