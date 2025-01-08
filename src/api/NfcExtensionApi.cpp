/**
 *
 *  Copyright 2024-2025 NXP
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

#include "DefaultEventHandler.h"
#include "NfcExtensionApi.h"
#include "MposHandler.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "ProprietaryExtn.h"
#include "QTagHandler.h"
#include <phNxpLog.h>

VendorExtnCb *mVendorExtnCb;

/**************** local methods used in this file only ************************/
/**
 * @brief handles the vendor NCI message
 * @param dataLen length of the NCI packet
 * @param pData pointer of the NCI packet
 * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
 * feature and handled by extension library otherwise
 * NFCSTATUS_EXTN_FEATURE_FAILURE.
 */
static NFCSTATUS phNxpExtn_HandleVendorNciMsg(uint16_t *dataLen,
                                              const uint8_t *pData);

/**
 * @brief handles the vendor NCI response and notification
 * @param dataLen length of the NCI packet
 * @param pData pointer of the NCI packet
 * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
 * feature and handled by extension library otherwise
 * NFCSTATUS_EXTN_FEATURE_FAILURE. \Note Handler implements this function is
 * responsible to call base class phNxpExtn_HandleVendorNciRspNtf to stop the
 * response timer.
 *
 */
static NFCSTATUS phNxpExtn_HandleVendorNciRspNtf(uint16_t *dataLen,
                                                 const uint8_t *pData);

/**
 * @brief provide write status callback
 * @return void
 *
 */
static void phNxpExtn_OnWriteComplete(uint8_t status);

/**
 * @brief Called by libnfc-nci when NFCC control is granted to HAL.
 * @return void
 *
 */
static void phNxpExtn_OnHalControlGranted();

/**
 * @brief Updates the Nfc Hal state to
 *        extension library
 * @return void
 *
 */
static void phNxpExtn_OnNfcHalStateUpdate(uint8_t halState);

/**
 * @brief Updates the Nfc RF state to
 *        extension library
 * @return void
 *
 */
static void phNxpExtn_OnRfStateUpdate(uint8_t rfState);

/**
 * @brief Updates the FW DNLD staus to
 *        extension library
 * @return void
 *
 */
static void phNxpExtn_OnFwDnldStatusUpdate(uint8_t status);

/**
 * @brief HAL updates the failure and error
 *        events through this function
 * @param event specifies events
 *        defined in NfcExtensionConstants
 * @param event_status event status
 * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
 * feature and handled by extension library otherwise
 * NFCSTATUS_EXTN_FEATURE_FAILURE.
 *
 */
static NFCSTATUS phNxpExtn_OnHandleHalEvent(uint8_t event,
                                            uint8_t event_status);

/**
 * @brief Add the list of supported event handlers
 *        to controller.
 * \note While adding the new feature support, corresponding
 *       feature/event handler to be added here.
 * @return void
 *
 */
static void addHandlers() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  std::shared_ptr<IEventHandler> defaultEventHandler =
      std::make_shared<DefaultEventHandler>();
  NfcExtensionController::getInstance()->addEventHandler(HandlerType::DEFAULT,
                                                         defaultEventHandler);
  NfcExtensionController::getInstance()->addEventHandler(
      HandlerType::MPOS, std::make_shared<MposHandler>());
  NfcExtensionController::getInstance()->addEventHandler(
      HandlerType::QTAG, std::make_shared<QTagHandler>());
}

static void printGenExtnLibVersion() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "NXP_GEN_EXT Version:%02X_%02X",
                 NFC_NXP_GEN_EXT_VERSION_MAJ, NFC_NXP_GEN_EXT_VERSION_MIN);
}

void phNxpExtn_LibInit(VendorExtnCb *vendorExtnCb) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mVendorExtnCb = vendorExtnCb;
  printGenExtnLibVersion();
  ProprietaryExtn::getInstance()->setupPropExtension(IS_SYS_EXT);
  NfcExtensionController::getInstance()->init();
  addHandlers();
}

VendorExtnCb *getNfcVendorExtnCb() { return mVendorExtnCb; }

void phNxpExtn_LibDeInit() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  NfcExtensionController::getInstance()->switchEventHandler(
      HandlerType::DEFAULT);
}

NFCSTATUS phNxpExtn_HandleNfcEvent(NfcExtEvent_t eventCode,
                                   NfcExtEventData_t eventData) {
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter eventCode:%d", __func__,
                 eventCode);
  switch (eventCode) {
  case HANDLE_VENDOR_NCI_MSG: {
    status = phNxpExtn_HandleVendorNciMsg(eventData.nci_msg.data_len,
                                          eventData.nci_msg.p_data);
    break;
  }
  case HANDLE_VENDOR_NCI_RSP_NTF: {
    status = phNxpExtn_HandleVendorNciRspNtf(eventData.nci_rsp_ntf.data_len,
                                             eventData.nci_rsp_ntf.p_data);
    break;
  }
  case HANDLE_WRITE_COMPLETE_STATUS: {
    phNxpExtn_OnWriteComplete(eventData.write_status);
    break;
  }
  case HANDLE_HAL_CONTROL_GRANTED: {
    phNxpExtn_OnHalControlGranted();
    break;
  }
  case HANDLE_NFC_HAL_STATE_UPDATE: {
    phNxpExtn_OnNfcHalStateUpdate(eventData.hal_state);
    break;
  }
  case HANDLE_RF_HAL_STATE_UPDATE: {
    phNxpExtn_OnRfStateUpdate(eventData.rf_state);
    break;
  }
  case HANDLE_FW_DNLD_STATUS_UPDATE: {
    phNxpExtn_OnFwDnldStatusUpdate(eventData.hal_event_status);
    break;
  }
  case HANDLE_HAL_EVENT: {
    status = phNxpExtn_OnHandleHalEvent(eventData.hal_event,
                                        eventData.hal_event_status);
    break;
  }
  default: {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Invalid eventCode. Not handled!!", __func__);
    break;
  }
  }
  return status;
}

bool isVendorSpecificCmd(uint16_t dataLen, const uint8_t *pData) {
  if (dataLen > MIN_HED_LEN && (pData[NCI_GID_INDEX] == NCI_PROP_CMD_VAL) &&
      (pData[NCI_OID_INDEX] == NCI_PROP_NTF_ANDROID_OID ||
       pData[NCI_OID_INDEX] == NCI_ROW_PROP_OID_VAL ||
       pData[NCI_OID_INDEX] == NCI_OEM_PROP_OID_VAL)) {
    return true;
  }
  return false;
}

NFCSTATUS phNxpExtn_HandleVendorNciMsg(uint16_t *dataLen,
                                       const uint8_t *pData) {
  if (*dataLen <= NCI_PAYLOAD_LEN_INDEX || pData == nullptr) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Invalid Data. Not handled!!",
                   __func__);
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  if (isVendorSpecificCmd(*dataLen, pData)) {
    NFCSTATUS status =
        ProprietaryExtn::getInstance()->handleVendorNciMsg(*dataLen, pData);
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s prop status:%d", __func__,
                   static_cast<int>(status));
    if (status == NFCSTATUS_EXTN_FEATURE_SUCCESS) {
      return status;
    } else {
      status = NfcExtensionController::getInstance()->handleVendorNciMessage(
          *dataLen, pData);
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s gen status:%d", __func__,
                     static_cast<int>(status));
      return status;
    }
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Not vendor specific command!!",
                   __func__);
    phNxpExtn_Write(dataLen, (uint8_t *)pData);
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS phNxpExtn_HandleVendorNciRspNtf(uint16_t *dataLen,
                                          const uint8_t *pData) {
  if (*dataLen <= NCI_PAYLOAD_LEN_INDEX || pData == nullptr) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Invalid Data. Not handled!!",
                   __func__);
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  if (PlatformAbstractionLayer::getInstance()->palGetChipType() ==
      DEFAULT_CHIP_TYPE) {
    PlatformAbstractionLayer::getInstance()->processChipType((uint8_t *)pData,
                                                            *dataLen);
  }
  NFCSTATUS status =
      ProprietaryExtn::getInstance()->handleVendorNciRspNtf(*dataLen, pData);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s prop status:%d", __func__,
                 static_cast<int>(status));
  if (status == NFCSTATUS_EXTN_FEATURE_SUCCESS) {
    return status;
  } else {
    status = NfcExtensionController::getInstance()->handleVendorNciRspNtf(
        *dataLen, pData);
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s gen status:%d", __func__,
                   static_cast<int>(status));
    return status;
  }
}

void phNxpExtn_OnWriteComplete(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
  ProprietaryExtn::getInstance()->onExtWriteComplete(status);
  NfcExtensionController::getInstance()->onWriteCompletion(status);
}

void phNxpExtn_OnHalControlGranted() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s NfcExtensionApi Enter",
                 __func__);
  NfcExtensionController::getInstance()->halControlGranted();
}

void phNxpExtn_OnNfcHalStateUpdate(uint8_t halState) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter halState:%d", __func__,
                 halState);
  NfcExtensionController::getInstance()->updateNfcHalState(halState);
}

void phNxpExtn_OnRfStateUpdate(uint8_t rfState) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter rfState:%d", __func__,
                 rfState);
  NfcExtensionController::getInstance()->updateRfState(rfState);
}

void phNxpExtn_OnFwDnldStatusUpdate(uint8_t status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter status:%d", __func__,
                 status);
  ProprietaryExtn::getInstance()->updateFwDnlsStatus(status);
}

NFCSTATUS phNxpExtn_OnHandleHalEvent(uint8_t event, uint8_t event_status) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter event:%d", __func__,
                 event);
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;
  switch (event) {
  case HAL_NFC_FW_UPDATE_STATUS_EVT:
    phNxpExtn_OnFwDnldStatusUpdate(event_status);
    status = NFCSTATUS_EXTN_FEATURE_SUCCESS;
    break;
  case HAL_NFC_ERROR_EVT:
    event = NFCC_HAL_TRANS_ERR_CODE;
  default:
    ProprietaryExtn::getInstance()->handleHalEvent(event);
    NfcExtensionController::getInstance()->onHandleHalEvent(event);
    break;
  }
  return status;
}

NFCSTATUS phNxpExtn_Write(uint16_t *dataLen, uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s NCI datalen:%d", __func__,
                 *dataLen);
  return NfcExtensionController::getInstance()->processExtnWrite(dataLen,
                                                                 pData);
}
