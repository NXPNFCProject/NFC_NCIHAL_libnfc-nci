/*
 *
 *  The original Work has been changed by NXP.
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
 */

#include "Mpos.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "NfcExtensionApi.h"
#include "PlatformAbstractionLayer.h"
#include <cstring>
#include <phNfcNciConstants.h>
#include <phNxpLog.h>
#include <stdlib.h>
#include <string.h>

Mpos *Mpos::instance = nullptr;

Mpos::Mpos() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mState = MPOS_STATE_IDLE;
  isActivatedNfcRcv = false;
  isTimerStarted = false;
  tagOperationTimeout = 0x00;
  mTimeoutMaxCount = 0x00;
  RdrReqGuardTimer = 0x00;
  lastScrState = MPOS_STATE_UNKNOWN;
  mCurrentTimeoutCount = 0x00;
}

Mpos::~Mpos() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mState = MPOS_STATE_UNKNOWN;
}

Mpos *Mpos::getInstance() {
  if (instance == nullptr) {
    instance = new Mpos();
  }
  return instance;
}

void Mpos::updateState(ScrState_t state) {
  if (state < MPOS_STATE_MAX) {
    mState = state;
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : state updated to %s",
                   __func__, scrStateToString(mState).c_str());
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : %s [%d]", __func__,
                   scrStateToString(state).c_str(), state);
  }
}

ScrState_t Mpos::getState() { return mState; }

static void notifyReaderModeActionEvt(uint8_t ntfType) {
  uint8_t rdrModeNtf[5] = {0x00};
  rdrModeNtf[0] = NCI_PROP_NTF_VAL;
  rdrModeNtf[1] = NCI_ROW_PROP_OID_VAL;
  rdrModeNtf[2] = NCI_RDR_MODE_NTF_PL_LEN;
  rdrModeNtf[3] = MPOS_READER_MODE_NTF_SUB_GIDOID_VAL;
  rdrModeNtf[4] = ntfType;
  PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
      sizeof(rdrModeNtf), rdrModeNtf);
}

static bool isValidNciConfig(const uint8_t *buffer, size_t length) {
  uint8_t selectProfile[] = {0x20, 0x02, 0x05, 0x01, 0xA0, 0x44, 0x01, 0x00};
  size_t expectedLength = sizeof(selectProfile);
  if (length != expectedLength) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "Error: Buffer length mismatch.");
    return false;
  }

  // Compare the buffer with the expected NCI config
  if (PlatformAbstractionLayer::getInstance()->palMemcmp(buffer, selectProfile,
                                                         expectedLength) == 0) {
    return true;
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "Error: Invalid configuration");
    return false;
  }
}

static void switchToDefaultHandler() {
  NfcExtensionWriter::getInstance()->releaseHALcontrol();
  NfcExtensionController::getInstance()->switchEventHandler(
      HandlerType::DEFAULT);
}

static void sendSelectProfileCmd() {
  uint8_t selProfileCnfig[SET_CONFIG_BUFF_MAX_SIZE];
  long bufflen = SET_CONFIG_BUFF_MAX_SIZE;
  long retlen = 0;
  bool isfound =
      PlatformAbstractionLayer::getInstance()->palGetNxpByteArrayValue(
          NAME_NXP_PROP_RESET_EMVCO_CMD, (char *)selProfileCnfig, bufflen,
          &retlen);

  if (isfound && isValidNciConfig(selProfileCnfig, bufflen)) {
    selProfileCnfig[bufflen - 1] = 0x01;
    Mpos::getInstance()->updateState(
        MPOS_STATE_WAIT_FOR_PROFILE_SELECT_RESPONSE);
    NFC_WRITE(selProfileCnfig, bufflen);
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - START_FAIL");
    Mpos::getInstance()->updateState(MPOS_STATE_IDLE);
    notifyReaderModeActionEvt(ACTION_SE_READER_TAG_DISCOVERY_START_FAILED);
    NfcExtensionWriter::getInstance()->releaseHALcontrol();
  }
}

static void notifyRfDiscoveryStarted() {
  Mpos::getInstance()->updateState(MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION);
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - START SUCCESS");
  if (Mpos::getInstance()->tagOperationTimer.set(
          Mpos::getInstance()->tagOperationTimeout * 1000, NULL,
          Mpos::getInstance()->cardTapTimeoutHandler,
          &Mpos::getInstance()->tagOperationTimerId)) {
    notifyReaderModeActionEvt(ACTION_SE_READER_TAG_DISCOVERY_STARTED);
  } else {
    NfcExtensionController::getInstance()
        ->getCurrentEventHandler()
        ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
    NfcExtensionController::getInstance()->switchEventHandler(
        HandlerType::DEFAULT);
  }
}

static void notifyTagActivated() {
  Mpos::getInstance()->tagOperationTimer.kill(
      Mpos::getInstance()->tagOperationTimerId);
  Mpos::getInstance()->updateState(MPOS_STATE_RF_DISCOVERY_REQUEST_REMOVE);
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - ACTIVATED");
  notifyReaderModeActionEvt(ACTION_SE_READER_TAG_ACTIVATED);
}

static void notifyRfDiscoveryStopped() {
  Mpos::getInstance()->tagRemovalTimer.kill(
      Mpos::getInstance()->tagRemovalTimerId);
  Mpos::getInstance()->tagOperationTimer.kill(
      Mpos::getInstance()->tagOperationTimerId);
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - STOP_SUCCESS");
  notifyReaderModeActionEvt(ACTION_SE_READER_STOPPED);
}

static void sendDeselectProfileCmd() {
  uint8_t selProfileCnfig[SET_CONFIG_BUFF_MAX_SIZE];
  long bufflen = SET_CONFIG_BUFF_MAX_SIZE;
  long retlen = 0;
  bool isfound =
      PlatformAbstractionLayer::getInstance()->palGetNxpByteArrayValue(
          NAME_NXP_PROP_RESET_EMVCO_CMD, (char *)selProfileCnfig, bufflen,
          &retlen);

  if (isfound && isValidNciConfig(selProfileCnfig, bufflen)) {
    selProfileCnfig[bufflen - 1] = 0x00;
    Mpos::getInstance()->updateState(
        MPOS_STATE_WAIT_FOR_PROFILE_DESELECT_CFG_RSP);
    NFC_WRITE(selProfileCnfig, bufflen);
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - REQUESTED_FAIL");
    Mpos::getInstance()->updateState(MPOS_STATE_IDLE);
    notifyReaderModeActionEvt(ACTION_SE_READER_STOP_FAILED);
    switchToDefaultHandler();
  }
}

static void notifySeReaderRestarted() {
  Mpos::getInstance()->tagOperationTimer.kill(
      Mpos::getInstance()->tagOperationTimerId);
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - RESTART");
  Mpos::getInstance()->updateState(MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION);
  notifyReaderModeActionEvt(ACTION_SE_READER_TAG_DISCOVERY_RESTARTED);
  if (Mpos::getInstance()->tagOperationTimer.set(
          Mpos::getInstance()->tagOperationTimeout * 1000, NULL,
          Mpos::getInstance()->cardTapTimeoutHandler,
          &Mpos::getInstance()->tagOperationTimerId) == false) {
    NfcExtensionController::getInstance()
        ->getCurrentEventHandler()
        ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
    NfcExtensionController::getInstance()->switchEventHandler(
        HandlerType::DEFAULT);
  }
}

NFCSTATUS Mpos::processMposNciRspNtf(const std::vector<uint8_t> &pData) {
  ScrState_t state = getScrState(pData);
  if (state == MPOS_STATE_UNKNOWN) {
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  updateState(state);
  return processMposEvent(state);
}

ScrState_t Mpos::getScrState(const std::vector<uint8_t> &pData) {

  if (pData.size() < (NCI_HEADER_LEN - 1)) {
    return MPOS_STATE_UNKNOWN;
  }

  uint16_t mGidOid = ((pData[0] << 8) | pData[1]);

  switch (mGidOid) {
  case NCI_DISC_REQ_NTF_GID_OID:
    switch (mState) {
    case MPOS_STATE_START_IN_PROGRESS:
      return MPOS_STATE_REQUEST_HAL_CONTROL;
    case MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION:
    case MPOS_STATE_RF_DISCOVERY_REQUEST_REMOVE:
      return MPOS_STATE_NOTIFY_STOP_RF_DISCOVERY;
    case MPOS_STATE_WAIT_FOR_RF_DEACTIVATION:
      return MPOS_STATE_SE_READER_STOPPED;
    case MPOS_STATE_SE_READER_STOPPED:
    case MPOS_STATE_IDLE:
    case MPOS_STATE_GENERIC_NTF:
      return MPOS_STATE_GENERIC_NTF;
    default:
      break;
    }
    break;

  case NCI_SET_CONFIG_RSP_GID_OID:
    switch (mState) {
    case MPOS_STATE_WAIT_FOR_PROFILE_SELECT_RESPONSE:
      return MPOS_STATE_SEND_DISCOVER_MAP_CMD;
    case MPOS_STATE_WAIT_FOR_PROFILE_DESELECT_CFG_RSP:
      return MPOS_STATE_SEND_DEFAULT_DISCOVERY_MAP_COMMAND;
    default:
      break;
    }
    break;

  case NCI_DISC_MAP_RSP_GID_OID:
    switch (mState) {
    case MPOS_STATE_WAIT_FOR_DISCOVER_MAP_RSP:
      return MPOS_STATE_NOTIFY_DISCOVER_MAP_RSP;
    case MPOS_STATE_WAIT_FOR_DEFAULT_DISCOVERY_MAP_RSP:
      return MPOS_STATE_RELEASE_HAL_CONTROL;
    default:
      break;
    }
    break;

  case NCI_RF_DISC_RSP_GID_OID:
    if (mState == MPOS_STATE_WAIT_FOR_RF_DISCOVERY_RSP) {
      return MPOS_STATE_DISCOVERY_STARTED;
    }
    break;

  case NCI_RF_INTF_ACTD_NTF_GID_OID:
    if (mState == MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION) {
      isActivatedNfcRcv = true;
      return MPOS_STATE_TRANSACTION_STARTED;
    }
    break;
  case NCI_RF_DEACTD_RSP_GID_OID:
    if ((mState == MPOS_STATE_WAIT_FOR_RF_DEACTIVATION) &&
        (isActivatedNfcRcv == false)) {
      return MPOS_STATE_SE_READER_STOPPED;
    } else if (mState == MPOS_STATE_RECOVERY_ON_EXT_FIELD_DETECT) {
      return MPOS_STATE_RECOVERY_ON_EXT_FIELD_DETECT;
    }
    break;
  case NCI_RF_DEACTD_NTF_GID_OID:
    if (mState == MPOS_STATE_WAIT_FOR_RF_DEACTIVATION) {
      return MPOS_STATE_SE_READER_STOPPED;
    }
    break;
  case NCI_GENERIC_ERR_NTF_GID_OID:{
    uint16_t status = pData[3];
    if (((mState == MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION) ||
        (mState == MPOS_STATE_GENERIC_ERR_NTF)) &&
        (status == NCI_TAG_COLLISION_DETECTED)) {
      return MPOS_STATE_GENERIC_ERR_NTF;
    }
    break;
  }
  case NCI_PROP_SYSTEM_GENERIC_INFO_NTF_GID_OID: {
    if (isExternalFieldDetected(pData)) {
      return MPOS_STATE_EXT_FIELD_DETECTED_DURING_POLL;
    }
    break;
  }
  case NCI_DATA_PKT_RSP_GID_OID: {
    if ((isSeReaderRestarted(pData) == true) && (mState != MPOS_STATE_IDLE)) {
      return MPOS_STATE_DISCOVERY_RESTARTED;
    } else {
      return MPOS_STATE_UNKNOWN;
    }
    break;
  }
  default:
    return MPOS_STATE_UNKNOWN;
  }
  return MPOS_STATE_UNKNOWN;
}

void Mpos::cardTapTimeoutHandler(union sigval val) {
  UNUSED_PROP(val);
  NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - Timeout");

  getInstance()->tagOperationTimer.kill(getInstance()->tagOperationTimerId);
  getInstance()->updateState(MPOS_STATE_NO_TAG_TIMEOUT);
  getInstance()->processMposEvent(MPOS_STATE_NO_TAG_TIMEOUT);
  return;
}

void Mpos::cardRemovalTimeoutHandler(union sigval val) {
  UNUSED_PROP(val);
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "SWP READER - REMOVE_CARD");

  notifyReaderModeActionEvt(ACTION_SE_READER_TAG_REMOVE_TIMEOUT);

  if (getInstance()->mTimeoutMaxCount != 0x00) {
    /* Increment the counter for remove Tag event*/
    getInstance()->mCurrentTimeoutCount++;
    /* If match with defined max count in the config file trigger the recovery
     */
    if (getInstance()->mCurrentTimeoutCount ==
        getInstance()->mTimeoutMaxCount) {
      getInstance()->mCurrentTimeoutCount = 0x00;
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:DISC_NTF_TIMEOUT", __func__);
      NfcExtEventData_t eventData;
      eventData.hal_event = NFCC_HAL_FATAL_ERR_CODE;
      vendor_nfc_handle_event(HANDLE_HAL_EVENT, eventData);
      abort();
    }
  }

  if (getInstance()->getState() == MPOS_STATE_WAIT_FOR_RF_DEACTIVATION) {
    if (Mpos::getInstance()->tagRemovalTimer.set(
            READER_MODE_CARD_REMOVE_TIMEOUT_IN_MS, NULL,
            cardRemovalTimeoutHandler,
            &getInstance()->tagRemovalTimerId) == false) {
      NfcExtensionController::getInstance()
          ->getCurrentEventHandler()
          ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
      NfcExtensionController::getInstance()->switchEventHandler(
          HandlerType::DEFAULT);
    }
  }
  return;
}

void Mpos::guardTimeoutHandler(union sigval val) {
  UNUSED_PROP(val);
  getInstance()->processMposEvent(getInstance()->lastScrState);
}

bool Mpos::isExternalFieldDetected(const std::vector<uint8_t> &pData) {
  int index = 3;
  // Get number of TLVS
  uint8_t numOfParams = pData[index++];
  while (numOfParams > 0) {
    // Parse all TLVS
    uint8_t tag = pData[index++];
    uint8_t len = pData[index++];
    std::vector<uint8_t> value((pData.data() + index),
                               (pData.data() + index + len));
    index += len;
    numOfParams--;
    if (tag == (uint8_t)NfcCorePropRFMgmtSysGenInfoTags::EMVCO_PCD &&
        len == 0x01 &&
        value[0] == (uint8_t)NfcCorePropRfMgmtSysGenInfoValues::
                        EXTENDED_FIELD_DETECTED_DURING_POLL) {
      return true;
    }
  }
  return false;
}

void Mpos::recoveryTimeoutHandler(union sigval val) {
  UNUSED_PROP(val);
  NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "MPOS RECOVERY WAIT COMPLETED");
  NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "Restarting Rf Discovery for Mpos");
  getInstance()->updateState(MPOS_STATE_WAIT_FOR_RF_DISCOVERY_RSP);
  notifyReaderModeActionEvt(ACTION_SE_READER_START_RF_DISCOVERY);
  return;
}

bool Mpos::isSeReaderRestarted(const std::vector<uint8_t> &pData) {

  if (pData.size() < 0x05) {
    return false;
  }
  /* Move the index forward by 5 bytes (3bytes NCI header +
   2bytes pipeID, Cmd, instruction and length field*/
  uint8_t aid = pData[5];
  uint8_t aidLength = pData[6];

  if ((aid != NCI_AID_VAL) || (aidLength != NCI_AID_LEN)) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "Unexpected AID[0x%X] or length[0x%X]\n", aid, aidLength);
    return false;
  }
  /* Move the index forward by 7 bytes (3bytes NCI header +
   2bytes pipeID & other + 2bytes AID & length field )*/
  uint8_t paramType = pData[7 + aidLength];
  uint8_t paramLength = pData[8 + aidLength];
  if ((paramType != NCI_PARAM_TYPE) || (paramLength != NCI_PARAM_TYPE_LEN)) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "Unexpected ParamType[0x%X] or Len[0x%X] \n", paramType,
                   paramLength);
    return false;
  }

  /* Move the index forward by 9 bytes (3bytes NCI header +
   2bytes pipeID & other + 2bytes AID & length field + 2bytes paramType & len)*/
  uint8_t Event = pData[9 + aidLength];
  uint8_t Version = pData[10 + aidLength];
  uint8_t Code = pData[11 + aidLength];

  if ((Event == EVENT_MPOS_RF_ERROR) && (Version == EVENT_MPOS_RF_VERSION) &&
      (Code == EVENT_MPOS_RDR_MODE_RESTART)) {
    return true;
  }

  return false;
}

bool Mpos::isProcessRdrReq(ScrState_t state) {
  bool isProcRdrReq = true;
  NFCSTATUS wStatus;
  if (IS_START_STOP_STATE_REQUEST(state)) {
    if (isTimerStarted) {
      /* Stop guard timer & continue with normal seq */
      getInstance()->startStopGuardTimer.kill(startStopGuardTimerId);
      isTimerStarted = false;
      lastScrState = MPOS_STATE_UNKNOWN;
    } else {
      /* save the event & start the timer */
      lastScrState = state;
      isTimerStarted = true;
      /* Start timer on expire process the last reader requested event */
      if (getInstance()->startStopGuardTimer.set(
              RdrReqGuardTimer, NULL, guardTimeoutHandler, &startStopGuardTimerId) == false) {
        NfcExtensionController::getInstance()
            ->getCurrentEventHandler()
            ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
        NfcExtensionController::getInstance()->switchEventHandler(
            HandlerType::DEFAULT);
      }

      isProcRdrReq = false;
    }
  }
  return isProcRdrReq;
}

NFCSTATUS Mpos::processMposEvent(ScrState_t state) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : Current state %s", __func__,
                 scrStateToString(mState).c_str());
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;

  if (RdrReqGuardTimer && !isProcessRdrReq(state)) {
    return NFCSTATUS_EXTN_FEATURE_SUCCESS; /* Guard timer has been started */
  }

  switch (state) {
  case MPOS_STATE_REQUEST_HAL_CONTROL: {
    NfcExtensionWriter::getInstance()->requestHALcontrol();
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_SEND_PROFILE_SELECT_CONFIG_CMD: {
    sendSelectProfileCmd();
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_SEND_DISCOVER_MAP_CMD: {
    updateState(MPOS_STATE_WAIT_FOR_DISCOVER_MAP_RSP);
    uint8_t dis_map_cmd[] = {0x21, 0x00, 0x04, 0x01, 0x04, 0x01, 0x83};
    NFC_WRITE(dis_map_cmd, sizeof(dis_map_cmd));
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_NOTIFY_DISCOVER_MAP_RSP: {
    updateState(MPOS_STATE_WAIT_FOR_RF_DISCOVERY_RSP);
    NfcExtensionWriter::getInstance()->releaseHALcontrol();
    notifyReaderModeActionEvt(ACTION_SE_READER_START_RF_DISCOVERY);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_DISCOVERY_STARTED: {
    notifyRfDiscoveryStarted();
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  case MPOS_STATE_TRANSACTION_STARTED: {
    notifyTagActivated();
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_NOTIFY_STOP_RF_DISCOVERY: {
    updateState(MPOS_STATE_WAIT_FOR_RF_DEACTIVATION);
    if (tagRemovalTimer.set(READER_MODE_CARD_REMOVE_TIMEOUT_IN_MS, NULL,
                            cardRemovalTimeoutHandler, &tagRemovalTimerId)) {
      notifyReaderModeActionEvt(ACTION_SE_READER_STOP_RF_DISCOVERY);
    } else {
      NfcExtensionController::getInstance()
          ->getCurrentEventHandler()
          ->notifyGenericErrEvt(NCI_UN_RECOVERABLE_ERR);
      NfcExtensionController::getInstance()->switchEventHandler(
          HandlerType::DEFAULT);
    }
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_SE_READER_STOPPED: {
    mCurrentTimeoutCount = 0x00; /* Card removed - clear the flag*/
    notifyRfDiscoveryStopped();
    if (isActivatedNfcRcv == false) {
      return NFCSTATUS_EXTN_FEATURE_FAILURE;
    } else {
      isActivatedNfcRcv = false;
      return NFCSTATUS_EXTN_FEATURE_SUCCESS;
    }
  }
  case MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD: {
    sendDeselectProfileCmd();
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_SEND_DEFAULT_DISCOVERY_MAP_COMMAND: {
    uint8_t default_map_cmd[] = {0x21, 0x00, 0x0A, 0x03, 0x04, 0x03, 0x02,
                                 0x03, 0x02, 0x01, 0x80, 0x01, 0x80};
    updateState(MPOS_STATE_WAIT_FOR_DEFAULT_DISCOVERY_MAP_RSP);
    NFC_WRITE(default_map_cmd, sizeof(default_map_cmd));
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_RELEASE_HAL_CONTROL: {
    updateState(MPOS_STATE_IDLE);
    notifyReaderModeActionEvt(ACTION_SE_READER_START_DEFAULT_RF_DISCOVERY);
    switchToDefaultHandler();
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_NO_TAG_TIMEOUT: {
    updateState(MPOS_STATE_RF_DISCOVERY_REQUEST_REMOVE);
    notifyReaderModeActionEvt(ACTION_SE_READER_TAG_TIMEOUT);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_GENERIC_ERR_NTF: {
    getInstance()->tagOperationTimer.kill(tagOperationTimerId);
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "SWP READER - MULTIPLE_TARGET_DETECTED");
    notifyReaderModeActionEvt(ACTION_SE_READER_MULTIPLE_TAG_DETECTED);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_DISCOVERY_RESTARTED: {
    notifySeReaderRestarted();
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  case MPOS_STATE_EXT_FIELD_DETECTED_DURING_POLL: {
    NXPLOG_EXTNS_D(
        NXPLOG_ITEM_NXP_GEN_EXTN,
        "EXTERNAL FIELD DETECTED DURING POLL, STOPPING RF DISCOVERY");
    updateState(MPOS_STATE_RECOVERY_ON_EXT_FIELD_DETECT);
    notifyReaderModeActionEvt(ACTION_SE_READER_STOP_RF_DISCOVERY);
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  case MPOS_STATE_RECOVERY_ON_EXT_FIELD_DETECT: {
    // Start a timer to restart rf discovery after timeout
    if (!mRecoveryTimer.set(recoveryTimeout * 1000, NULL,
                            recoveryTimeoutHandler, &mRecoveryTimerId)) {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "Failed to start MPOS_RECOVERY_TIMER");
    } else {
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "MPOS_RECOVERY_TIMER STARTED");
    }
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  case MPOS_STATE_GENERIC_NTF:{
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  break;
  default: {
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  } break;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

std::string Mpos::scrStateToString(ScrState_t state) {
  switch (state) {
  case MPOS_STATE_IDLE:
    return "MPOS_STATE_IDLE";
  case MPOS_STATE_START_IN_PROGRESS:
    return "MPOS_STATE_START_IN_PROGRESS";
  case MPOS_STATE_REQUEST_HAL_CONTROL:
    return "MPOS_STATE_REQUEST_HAL_CONTROL";
  case MPOS_STATE_SEND_PROFILE_SELECT_CONFIG_CMD:
    return "MPOS_STATE_SEND_PROFILE_SELECT_CONFIG_CMD";
  case MPOS_STATE_WAIT_FOR_PROFILE_SELECT_RESPONSE:
    return "MPOS_STATE_WAIT_FOR_PROFILE_SELECT_RESPONSE";
  case MPOS_STATE_SEND_DISCOVER_MAP_CMD:
    return "MPOS_STATE_SEND_DISCOVER_MAP_CMD";
  case MPOS_STATE_WAIT_FOR_DISCOVER_MAP_RSP:
    return "MPOS_STATE_WAIT_FOR_DISCOVER_MAP_RSP";
  case MPOS_STATE_NOTIFY_DISCOVER_MAP_RSP:
    return "MPOS_STATE_NOTIFY_DISCOVER_MAP_RSP";
  case MPOS_STATE_WAIT_FOR_RF_DISCOVERY_RSP:
    return "MPOS_STATE_WAIT_FOR_RF_DISCOVERY_RSP";
  case MPOS_STATE_DISCOVERY_STARTED:
    return "MPOS_STATE_DISCOVERY_STARTED";
  case MPOS_STATE_DISCOVERY_RESTARTED:
    return "MPOS_STATE_DISCOVERY_RESTARTED";
  case MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION:
    return "MPOS_STATE_WAIT_FOR_INTERFACE_ACTIVATION";
  case MPOS_STATE_TRANSACTION_STARTED:
    return "MPOS_STATE_TRANSACTION_STARTED";
  case MPOS_STATE_RF_DISCOVERY_REQUEST_REMOVE:
    return "MPOS_STATE_RF_DISCOVERY_REQUEST_REMOVE";
  case MPOS_STATE_NOTIFY_STOP_RF_DISCOVERY:
    return "MPOS_STATE_NOTIFY_STOP_RF_DISCOVERY";
  case MPOS_STATE_WAIT_FOR_RF_DEACTIVATION:
    return "MPOS_STATE_WAIT_FOR_RF_DEACTIVATION";
  case MPOS_STATE_SE_READER_STOPPED:
    return "MPOS_STATE_SE_READER_STOPPED";
  case MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD:
    return "MPOS_STATE_SEND_PROFILE_DESELECT_CONFIG_CMD";
  case MPOS_STATE_WAIT_FOR_PROFILE_DESELECT_CFG_RSP:
    return "MPOS_STATE_WAIT_FOR_PROFILE_DESELECT_CFG_RSP";
  case MPOS_STATE_SEND_DEFAULT_DISCOVERY_MAP_COMMAND:
    return "MPOS_STATE_SEND_DEFAULT_DISCOVERY_MAP_COMMAND";
  case MPOS_STATE_WAIT_FOR_DEFAULT_DISCOVERY_MAP_RSP:
    return "MPOS_STATE_WAIT_FOR_DEFAULT_DISCOVERY_MAP_RSP";
  case MPOS_STATE_RELEASE_HAL_CONTROL:
    return "MPOS_STATE_RELEASE_HAL_CONTROL";
  case MPOS_STATE_HAL_CONTROL_REQUEST_TIMEOUT:
    return "MPOS_STATE_HAL_CONTROL_REQUEST_TIMEOUT";
  case MPOS_STATE_NO_TAG_TIMEOUT:
    return "MPOS_STATE_NO_TAG_TIMEOUT";
  case MPOS_STATE_GENERIC_ERR_NTF:
    return "MPOS_STATE_GENERIC_ERR_NTF";
  case MPOS_STATE_EXT_FIELD_DETECTED_DURING_POLL:
    return "MPOS_STATE_EXT_FIELD_DETECTED_DURING_POLL";
  case MPOS_STATE_RECOVERY_ON_EXT_FIELD_DETECT:
    return "MPOS_STATE_RECOVERY_ON_EXT_FIELD_DETECT";
  case MPOS_STATE_GENERIC_NTF:
    return "MPOS_STATE_GENERIC_NTF";
  case MPOS_STATE_UNKNOWN:
    return "MPOS_STATE_UNKNOWN";
  default:
    return "Unknown State";
  }
}
