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

//Important Do Not change the order of DefaultEventHandler.h it leads to build failure*
//The order is crtical for correct copilation

#include "DefaultEventHandler.h"
#include "NfcExtensionApi.h"
#include "LxDebugHandler.h"
#include "MposHandler.h"
#include "NciCommandBuilder.h"
#include "NfcExtensionConstants.h"
#include "NfcExtensionController.h"
#include "NfcFirmwareInfo.h"
#include "PlatformAbstractionLayer.h"
#include "ProprietaryExtn.h"
#include "QTagHandler.h"
#include "SrdHandler.h"
#include "TransitConfigHandler.h"
#include <phNxpLog.h>
#include <stdint.h>

VendorExtnCb *mVendorExtnCb;

#define NXP_EN_SN110U 1
#define NXP_EN_SN100U 1
#define NXP_EN_SN220U 1
#define NXP_EN_PN557 1
#define NXP_EN_PN560 1
#define NXP_EN_SN300U 1
#define NXP_EN_SN330U 1
#define NFC_NXP_MW_ANDROID_VER (16U)  /* Android version used by NFC MW */
#define NFC_NXP_MW_VERSION_MAJ (0x06) /* MW Major Version */
#define NFC_NXP_MW_VERSION_MIN (0x00) /* MW Minor Version */
#define NFC_NXP_MW_CUSTOMER_ID (0x00) /* MW Customer Id */
#define NFC_NXP_MW_RC_VERSION (0x00)  /* MW RC Version */

/**
 * @brief Extension events defined in system / vendor cannot be accessed
 * out side of this file. assign respective value for the extension variables
 * to make them accessible across the extension libraries
 *
 */
uint8_t EXT_NFC_ADAPTATION_INIT = HANDLE_NFC_ADAPTATION_INIT;
uint8_t EXT_NFC_PRE_DISCOVER = HANDLE_NFC_PRE_DISCOVER;

/**************** local methods used in this file only ************************/
/**
 * @brief handles the vendor NCI message
 * @param dataLen length of the NCI packet
 * @param pData pointer of the NCI packet
 * @return returns NFCSTATUS_EXTN_FEATURE_SUCCESS, if it is vendor specific
 * feature and handled by extension library otherwise
 * NFCSTATUS_EXTN_FEATURE_FAILURE.
 * @Note resend logic is handled in extension library for following NCI
 * commands- NCI_MSG_RF_DEACTIVATE, NCI_MSG_NFCEE_MODE_SET,
 * NCI_MSG_CORE_SET_POWER_SUB_STATE NCI_MSG_RF_DISCOVER,NCI_MSG_CORE_SET_CONFIG
 * - NCI_PARAM_ID_CON_DISCOVERY_PARAM
 */
static NFCSTATUS phNxpExtn_HandleVendorNciMsg(uint16_t dataLen,
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
static NFCSTATUS phNxpExtn_HandleVendorNciRspNtf(uint16_t dataLen,
                                                 uint8_t *pData);

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
 * @brief Send a response with the status 'Not Supported' (0x0B) for commands
 * whose implementation is not found.
 * @param subGidOid of the received command
 * @return None
 *
 */
static void phNxpExtn_SendNotSupportedRsp(uint8_t subGidOid);

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
  NfcExtensionController::getInstance()->addEventHandler(
      HandlerType::TRANSIT, std::make_shared<TransitConfigHandler>());
  NfcExtensionController::getInstance()->addEventHandler(
      HandlerType::FW, std::make_shared<NfcFirmwareInfo>());
  NfcExtensionController::getInstance()->addEventHandler(
      HandlerType::LxDebug, std::make_shared<LxDebugHandler>());
  NfcExtensionController::getInstance()->addEventHandler(
      HandlerType::SRD, std::make_shared<SrdHandler>());
}

static void printGenExtnLibVersion() {
  uint32_t validation = (NXP_EN_SN100U << 13);
  validation |= (NXP_EN_SN110U << 14);
  validation |= (NXP_EN_SN220U << 15);
  validation |= (NXP_EN_PN560 << 16);
  validation |= (NXP_EN_SN300U << 17);
  validation |= (NXP_EN_SN330U << 18);
  validation |= (NXP_EN_PN557 << 11);

  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                 "NXP_GEN_EXT Version: NXP_AR_%02X_%05X_%02d.%02x.%02x",
                 NFC_NXP_MW_CUSTOMER_ID, validation, NFC_NXP_MW_ANDROID_VER,
                 NFC_NXP_MW_VERSION_MAJ, NFC_NXP_MW_VERSION_MIN);
}

bool vendor_nfc_init(VendorExtnCb *vendorExtnCb) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mVendorExtnCb = vendorExtnCb;
  printGenExtnLibVersion();
  ProprietaryExtn::getInstance()->setupPropExtension(NXP_PROP_LIB_PATH);
  NfcExtensionController::getInstance()->init();
  addHandlers();
  phNxpUpdate_logLevel();
  return true;
}

VendorExtnCb *getNfcVendorExtnCb() { return mVendorExtnCb; }

bool vendor_nfc_de_init() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  NfcExtensionController::getInstance()->switchEventHandler(
      HandlerType::DEFAULT);
  NfcExtensionController::finalize();
  NciCommandBuilder::finalize();
  NfcExtensionWriter::finalize();
  PlatformAbstractionLayer::finalize();
  ProprietaryExtn::finalize();
  resetNxpConfig();
  return true;
}

NFCSTATUS vendor_nfc_handle_event(NfcExtEvent_t eventCode,
                                   NfcExtEventData_t eventData) {
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter eventCode:%d", __func__,
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
      (pData[NCI_OID_INDEX] == NCI_ROW_PROP_OID_VAL ||
       pData[NCI_OID_INDEX] == NCI_OEM_PROP_OID_VAL)) {
    return true;
  }
  return false;
}

NFCSTATUS phNxpExtn_HandleVendorNciMsg(uint16_t dataLen,
                                       const uint8_t *pData) {
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;
  if (dataLen <= NCI_PAYLOAD_LEN_INDEX || pData == nullptr) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Invalid Data. Not handled!!",
                   __func__);
    return status;
  }
  if (isVendorSpecificCmd(dataLen, pData)) {
    status =
        ProprietaryExtn::getInstance()->handleVendorNciMsg(dataLen, pData);
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s prop status:%d", __func__,
                   static_cast<int>(status));
    if (status == NFCSTATUS_EXTN_FEATURE_SUCCESS) {
      return status;
    } else {
      status = NfcExtensionController::getInstance()->handleVendorNciMessage(
          dataLen, pData);
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s gen status:%d", __func__,
                     static_cast<int>(status));
      if (status == NFCSTATUS_EXTN_FEATURE_SUCCESS) {
        return status;
      } else {
        phNxpExtn_SendNotSupportedRsp(pData[SUB_GID_OID_INDEX]);
        return NFCSTATUS_EXTN_FEATURE_SUCCESS;
      }
    }
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Not vendor specific command!!",
                   __func__);
    status = phNxpExtn_Write(dataLen, (uint8_t *)pData);
  }
  return status;
}

void phNxpExtn_SendNotSupportedRsp(uint8_t subGidOid) {
  vector<uint8_t> rsp{
      NCI_PROP_RSP_VAL,
      NCI_OEM_PROP_OID_VAL,
      PAYLOAD_TWO_LEN,
      subGidOid,
      RESPONSE_STATUS_OPERATION_NOT_SUPPORTED,
  };
  PlatformAbstractionLayer::getInstance()->palenQueueRspNtf(rsp.data(),
                                                            rsp.size());
}

NFCSTATUS phNxpExtn_HandleVendorNciRspNtf(uint16_t dataLen, uint8_t *pData) {
  if (dataLen <= NCI_PAYLOAD_LEN_INDEX || pData == nullptr) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Invalid Data. Not handled!!",
                   __func__);
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  if (PlatformAbstractionLayer::getInstance()->palGetChipType() ==
      DEFAULT_CHIP_TYPE) {
    PlatformAbstractionLayer::getInstance()->processChipType((uint8_t *)pData,
                                                            dataLen);
  }
  NFCSTATUS status =
      ProprietaryExtn::getInstance()->handleVendorNciRspNtf(dataLen, pData);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s prop status:%d", __func__,
                 static_cast<int>(status));
  if (status == NFCSTATUS_EXTN_FEATURE_SUCCESS) {
    return status;
  } else {
    status = NfcExtensionController::getInstance()->handleVendorNciRspNtf(
        dataLen, pData);
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
  case HANDLE_DOWNLOAD_FIRMWARE_REQUEST:
    event_status = STATUS_FW_DL_REQUEST;
    phNxpExtn_OnFwDnldStatusUpdate(event_status);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  case HAL_NFC_FW_UPDATE_STATUS_EVT:
    phNxpExtn_OnFwDnldStatusUpdate(event_status);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  case HAL_NFC_ERROR_EVT:
    event = NFCC_HAL_TRANS_ERR_CODE;
    break;
  case HANDLE_NFC_ADAPTATION_INIT:
    event = EXT_NFC_ADAPTATION_INIT;
    break;
  case HANDLE_NFC_PRE_DISCOVER:
    event = EXT_NFC_PRE_DISCOVER;
    break;
  default:
    break;
  }
  status = ProprietaryExtn::getInstance()->handleHalEvent(event);
  if (status == NFCSTATUS_EXTN_FEATURE_FAILURE) {
    status = NfcExtensionController::getInstance()->onHandleHalEvent(event);
  }
  return status;
}

NFCSTATUS phNxpExtn_Write(uint16_t dataLen, const uint8_t* pData) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s NCI datalen:%d", __func__,
                 dataLen);
  uint8_t *nciCmd = const_cast<uint8_t *>(pData);
  NFCSTATUS status =
      ProprietaryExtn::getInstance()->processExtnWrite(&dataLen, nciCmd);
  if (status == NFCSTATUS_EXTN_FEATURE_FAILURE) {
    status = NfcExtensionController::getInstance()->processExtnWrite(&dataLen,
                                                                     nciCmd);
  }
  return status;
}

void vendor_nfc_on_config_update(map<string, ConfigValue>* pConfigMap) {
  PlatformAbstractionLayer::getInstance()->updateHalConfig(pConfigMap);
}
