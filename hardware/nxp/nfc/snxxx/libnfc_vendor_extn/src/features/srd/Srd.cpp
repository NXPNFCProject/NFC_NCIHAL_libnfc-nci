/*
 *
 *  The original Work has been changed by NXP.
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
 */

#include "Srd.h"
#include "NfcExtensionApi.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include <cstring>
#include <phNfcNciConstants.h>
#include <phNxpLog.h>
#include <stdlib.h>
#include <string.h>

Srd *Srd::instance = nullptr;

Srd::Srd() { mState = SRD_STATE_IDLE; }

Srd::~Srd() { mState = SRD_STATE_IDLE; }

Srd *Srd::getInstance() {
  if (instance == nullptr) {
    instance = new Srd();
  }
  return instance;
}

static void switchToDefaultHandler() {
  NfcExtensionController::getInstance()->switchEventHandler(
      HandlerType::DEFAULT);
}

/*Initilize the SRD module*/
void Srd::Initilize() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  uint16_t SRD_DISABLE_MASK = 0xFFFF;
  uint16_t isFeatureDisable = 0x0000;
  uint8_t isfound = 0;
  const int NXP_SRD_TIMEOUT_BUF_LEN = 2;
  uint8_t *srd_config = NULL;
  long bufflen = 260;
  srd_config = (uint8_t *)malloc(bufflen * sizeof(uint8_t));
  long retlen = 0;
  memset(srd_config, 0x00, bufflen);
  isfound = GetNxpByteArrayValue(NAME_NXP_SRD_TIMEOUT, (char *)srd_config,
                                 bufflen, &retlen);
  if ((isfound)) {
    if (retlen == NXP_SRD_TIMEOUT_BUF_LEN) {
      isFeatureDisable = ((srd_config[1] << 8) & SRD_DISABLE_MASK);
      isFeatureDisable = (isFeatureDisable | srd_config[0]);
    }
  }
  if (!isfound || !isFeatureDisable) {
    /* NXP_SRD_TIMEOUT value not found in config file. */
    sIsSrdSupported = false;
  }
  if (sIsSrdSupported) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s SRD supported Exit", __func__);
  } else {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s SRD not supported Exit",
                   __func__);
  }
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s isFeatureDisable = %x Exit",
                 __func__, isFeatureDisable);
  if (srd_config != NULL) {
    free(srd_config);
    srd_config = NULL;
  }
}

void Srd::intSrd() { updateState(SRD_STATE_INIT); }

NFCSTATUS Srd::startSrdSeq() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : Entry", __func__);
  NFCSTATUS handleRespStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;
  if (mState == SRD_STATE_INIT) {
    handleRespStatus = sendModeSetCmd();
  }
  return handleRespStatus;
}

NFCSTATUS Srd::stopSrd() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : Entry", __func__);
  updateState(SRD_STATE_INIT);
  switchToDefaultHandler();
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

NFCSTATUS Srd::sendModeSetCmd() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : sendModeSetCmd", __func__);
  updateState(SRD_STATE_W4_MODE_SET_RSP);
  std::vector<uint8_t> cmd = {0x22, 0x01, 0x02, 0xC0, 0x01};
  mCmdBuffer.clear();
  mCmdBuffer.assign(cmd.begin(), cmd.end());
  NfcExtensionWriter::getInstance()->write(cmd.data(), cmd.size());
  return NFCSTATUS_EXTN_FEATURE_SUCCESS;
}

void Srd::notifySRDActionEvt(uint8_t ntfType) {

  uint8_t srdActNtf[5] = {0x00};
  srdActNtf[0] = NCI_PROP_NTF_VAL;
  srdActNtf[1] = NCI_ROW_PROP_OID_VAL;
  srdActNtf[2] = NCI_SDR_MODE_NTF_PL_LEN;
  srdActNtf[3] = SRD_ENABLE_DISCOVERY;
  srdActNtf[4] = ntfType;
  PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
      sizeof(srdActNtf), srdActNtf);
}

NFCSTATUS Srd::processSrdNciRspNtf(const std::vector<uint8_t> &pRspBuffer) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter mState = %d", __func__,
                 mState);
  NFCSTATUS handleRespStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;
  bool status = false;
  switch (mState) {
  case SRD_STATE_W4_MODE_SET_RSP:
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s : state = SRD_STATE_W4_MODE_SET_RSP", __func__);
    handleRespStatus = processModeSetRsp(pRspBuffer);
    break;
  case SRD_STATE_W4_MODE_SET_NTF:
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s : state = SRD_STATE_W4_MODE_SET_NTF", __func__);
    handleRespStatus = processModeSetNtf(pRspBuffer);
    break;
  case SRD_STATE_W4_HCI_CONNECTIVITY_EVT:
    if (checkSrdEvent(pRspBuffer) == SRD_START_EVT) {
      handleRespStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;
    }
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s : state = SRD_STATE_W4_HCI_CONNECTIVITY_EVT propagate to upper layer", __func__);
    break;
  case SRD_STATE_W4_DISCOVER_MAP_RSP:
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s : state = SRD_STATE_W4_DISCOVER_MAP_RSP", __func__);
    processDiscoverMapRsp(pRspBuffer);
    handleRespStatus = NFCSTATUS_EXTN_FEATURE_SUCCESS;
    break;
  case SRD_STATE_W4_RF_DISCOVERY_RSP:
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s : mState = %d  SRD_STATE_W4_RF_DISCOVERY_RSP", __func__,
                   mState);
    handleRespStatus = processRfDiscoverRsp(pRspBuffer);
    break;
  case SRD_STATE_W4_HCI_EVENT:
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s : mState = %d  SRD_STATE_W4_HCI_EVENT propagate to upper layer", __func__,
                   mState);
    if (checkSrdEvent(pRspBuffer) == SRD_STOP_EVT ||
        isTimeoutEvent(pRspBuffer)) {
      handleRespStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;
    }
    break;
  default:
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : Unkown SRD state %d",
                   __func__, mState);
    break;
  }
  return handleRespStatus;
}

NFCSTATUS Srd::processModeSetRsp(const std::vector<uint8_t> &pRspBuffer) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter", __func__);
  NFCSTATUS handleRespStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;
  if (mCmdBuffer.size() >= NCI_PAYLOAD_LEN_INDEX &&
      pRspBuffer.size() >= NCI_PAYLOAD_LEN_INDEX) {
    uint8_t cmdGid = (mCmdBuffer[NCI_GID_INDEX] & EXT_NCI_GID_MASK);
    uint8_t cmdOid = (mCmdBuffer[NCI_OID_INDEX] & EXT_NCI_OID_MASK);
    uint8_t rspGid = (pRspBuffer[NCI_GID_INDEX] & EXT_NCI_GID_MASK);
    uint8_t rspOid = (pRspBuffer[NCI_OID_INDEX] & EXT_NCI_OID_MASK);

    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Enter cmdGid:%d, cmdOid:%d, rspGid:%d, rspOid:%d,",
                   __func__, cmdGid, cmdOid, rspGid, rspOid);
    if (cmdGid == rspGid && cmdOid == rspOid) {
      updateState(SRD_STATE_W4_MODE_SET_NTF);
      mRspBuffer.insert(mRspBuffer.begin(), pRspBuffer.begin(),
                        pRspBuffer.end());
    }
    handleRespStatus = NFCSTATUS_EXTN_FEATURE_SUCCESS;
    uint8_t resp[] = {NCI_PROP_RSP_VAL, NCI_ROW_PROP_OID_VAL, PAYLOAD_TWO_LEN,
                      SRD_ENABLE_MODE, RESPONSE_STATUS_OK};
    PlatformAbstractionLayer::getInstance()->palSendNfcDataCallback(
        sizeof(resp), resp);
  }
  return handleRespStatus;
}

NFCSTATUS Srd::processModeSetNtf(const std::vector<uint8_t> &pRspBuffer) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter", __func__);
  NFCSTATUS handleRespStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;
  if (mCmdBuffer.size() >= NCI_PAYLOAD_LEN_INDEX &&
      pRspBuffer.size() >= NCI_PAYLOAD_LEN_INDEX) {
    uint8_t cmdGid = (mCmdBuffer[NCI_GID_INDEX] & EXT_NCI_GID_MASK);
    uint8_t cmdOid = (mCmdBuffer[NCI_OID_INDEX] & EXT_NCI_OID_MASK);
    uint8_t rspGid = (pRspBuffer[NCI_GID_INDEX] & EXT_NCI_GID_MASK);
    uint8_t rspOid = (pRspBuffer[NCI_OID_INDEX] & EXT_NCI_OID_MASK);

    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Enter cmdGid:%d, cmdOid:%d, rspGid:%d, rspOid:%d,",
                   __func__, cmdGid, cmdOid, rspGid, rspOid);
    if (cmdGid == rspGid && cmdOid == rspOid) {
      handleRespStatus = NFCSTATUS_EXTN_FEATURE_SUCCESS;
      updateState(SRD_STATE_W4_HCI_CONNECTIVITY_EVT);
    }
  }
  return handleRespStatus;
}

SrdEvent_t Srd::checkSrdEvent(const std::vector<uint8_t> &pDataPacket) {
  if (pDataPacket.size() >= NCI_PAYLOAD_LEN_INDEX) {
    uint8_t dataGid = (pDataPacket[NCI_GID_INDEX] & EXT_NCI_GID_MASK);
    uint8_t dataOid = (pDataPacket[NCI_OID_INDEX] & EXT_NCI_OID_MASK);

    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter dataGid:%d, dataOid:%d,",
                   __func__, dataGid, dataOid);
    if (dataGid == 0x01 && dataOid == 0x00) {
      return processSrdHciEvt(pDataPacket);
    }
  }
  return SRD_UNKOWN_EVT;
}

SrdEvent_t Srd::processSrdHciEvt(const std::vector<uint8_t> &pRspHciEvtData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter", __func__);
  bool status = false;
  static const uint8_t TAG_SRD_AID = 0x81;
  uint8_t event;
  constexpr uint8_t SRD_HCI_EVT_TRANSACTION = 0x12;
  constexpr uint8_t SRD_HCI_EVT_TRANSACTION_MASK = 0x1F;
  uint8_t event_size = pRspHciEvtData[2];
  event = pRspHciEvtData[4];
  if (((event & SRD_HCI_EVT_TRANSACTION_MASK) == SRD_HCI_EVT_TRANSACTION) &&
      (pRspHciEvtData[5] == TAG_SRD_AID) && (pRspHciEvtData[29] == 0x01)) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: HCI Transcation event",
                   __func__);
    srdSndDiscoverMapCmd();
    return SRD_START_EVT;
  } else if (((event & SRD_HCI_EVT_TRANSACTION_MASK) ==
              SRD_HCI_EVT_TRANSACTION) &&
             (pRspHciEvtData[5] == TAG_SRD_AID) &&
             (pRspHciEvtData[29] == 0x00)) {
    notifySRDActionEvt(RESTART_DEFAULT_DISCOVERY);
    stopSrd();
    return SRD_STOP_EVT;
  }
  return SRD_UNKOWN_EVT;
}

void Srd::processDiscoverMapRsp(const std::vector<uint8_t> &pRspHciEvtData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter", __func__);
  uint16_t mGidOid = ((pRspHciEvtData[0] << 8) | pRspHciEvtData[1]);
  switch (mGidOid) {
  case NCI_DISC_MAP_RSP_GID_OID:
    switch (mState) {
    case SRD_STATE_W4_DISCOVER_MAP_RSP:
      updateState(SRD_STATE_W4_RF_DISCOVERY_RSP);
      notifySRDActionEvt(START_SRD_DISCOVERY);
    default:
      break;
    }
    break;
  }
}

NFCSTATUS
Srd::processRfDiscoverRsp(const std::vector<uint8_t> &pRspHciEvtData) {
  NFCSTATUS handleRespStatus = NFCSTATUS_EXTN_FEATURE_FAILURE;
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter", __func__);
  uint16_t mGidOid = ((pRspHciEvtData[0] << 8) | pRspHciEvtData[1]);
  switch (mGidOid) {
  case NCI_RF_DISC_RSP_GID_OID:
    switch (mState) {
    case SRD_STATE_W4_RF_DISCOVERY_RSP:
      updateState(SRD_STATE_W4_HCI_EVENT);
    default:
      break;
    }
    break;
  }
  return handleRespStatus;
}

bool Srd::isTimeoutEvent(const std::vector<uint8_t> &pRspHciEvtData) {
  uint16_t mGidOid = ((pRspHciEvtData[0] << 8) | pRspHciEvtData[1]);
  switch (mGidOid) {
  case NCI_GENERIC_ERR_NTF_GID_OID:
    switch (mState) {
    case SRD_STATE_W4_HCI_EVENT:
      if ((pRspHciEvtData[2] == 0x01) && (pRspHciEvtData[3] == 0xE2)) {
        notifySRDActionEvt(SRD_TIMED_OUT);
        stopSrd();
        return true;
      }
    default:
      break;
    }
    break;
  }
  return false;
}

void Srd::srdSndDiscoverMapCmd() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s: enter", __func__);
  updateState(SRD_STATE_W4_DISCOVER_MAP_RSP);
  uint8_t dis_map_cmd[] = {0x21, 0x00, 0x07, 0x02, 0x02,
                           0x01, 0x01, 0x04, 0x01, 0x01};
  NFC_WRITE(dis_map_cmd, sizeof(dis_map_cmd));
}

void Srd::updateState(SrdState_t state) {
  if (state < SRD_STATE_MAX) {
    NXPLOG_EXTNS_D(
        NXPLOG_ITEM_NXP_GEN_EXTN, "%s : state updating from %s to %s", __func__,
        scrStateToString(mState).c_str(), scrStateToString(state).c_str());
    mState = state;
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : %s [%d]", __func__,
                   scrStateToString(state).c_str(), state);
  }
}

std::string Srd::scrStateToString(SrdState_t state) {
  switch (state) {
  case SRD_STATE_INIT:
    return "SRD_STATE_INIT";
  case SRD_STATE_IDLE:
    return "SRD_STATE_IDLE";
  case SRD_STATE_W4_MODE_SET_RSP:
    return "SRD_STATE_W4_MODE_SET_RSP";
  case SRD_STATE_W4_MODE_SET_NTF:
    return "SRD_STATE_W4_MODE_SET_NTF";
  case SRD_STATE_W4_HCI_CONNECTIVITY_EVT:
    return "SRD_STATE_W4_HCI_CONNECTIVITY_EVT";
  case SRD_STATE_W4_DISCOVER_MAP_RSP:
    return "SRD_STATE_W4_DISCOVER_MAP_RSP";
  case SRD_STATE_W4_RF_DISCOVERY_RSP:
    return "SRD_STATE_W4_RF_DISCOVERY_RSP";
  case SRD_STATE_W4_HCI_EVENT:
    return "SRD_STATE_W4_HCI_EVENT";
  default:
    return "Unknown State";
  }
}

NFCSTATUS Srd::processPowerLinkCmd(uint16_t dataLen, const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "Srd %s Enter", __func__);
  if (mState == SRD_STATE_IDLE)
    return NFCSTATUS_EXTN_FEATURE_FAILURE;

  vector<uint8_t> pwrLinkCmd;
  pwrLinkCmd.assign(pData, pData + (dataLen));
  uint16_t mGidOid = ((pwrLinkCmd[0] << 8) | pwrLinkCmd[1]);
  if (mGidOid == NCI_EE_PWR_LINK_CMD_GID_OID) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "Srd %s Enter PWR link cmd updated with 0x02", __func__);
    pwrLinkCmd.back() = 0x02;
    PlatformAbstractionLayer::getInstance()->palenQueueWrite(pwrLinkCmd.data(),
                                                             pwrLinkCmd.size());
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}
