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

#include "NciStateMonitor.h"
#include "NfceeStateMonitor.h"
#include "NfcExtensionConstants.h"
#include "RfStateMonitor.h"
#include <phNxpLog.h>
#include <stdio.h>

NciStateMonitor *NciStateMonitor::instance = nullptr;
static bool sSendSramConfigToFlash = false;

NciStateMonitor::NciStateMonitor() {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  mSramCmdRspBuffer.push_back(DEFAULT_RESP);
}

NciStateMonitor::~NciStateMonitor() {}

NciStateMonitor *NciStateMonitor::getInstance() {
  if (instance == nullptr) {
    instance = new NciStateMonitor();
  }
  return instance;
}

vector<uint8_t> NciStateMonitor::getFwVersion() { return firmwareVersion; }

NFCSTATUS NciStateMonitor::handleVendorNciMessage(uint16_t dataLen,
                                               const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "NciStateMonitor %s Enter dataLen:%d",
                 __func__, dataLen);
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS NciStateMonitor::processCoreGenericErrorNtf(
    vector<uint8_t> &coreGenericErrorNtf) {
  vector<uint8_t> propNtf = {0x6F, 0x0C, 0x01, 0x07};

  if ((coreGenericErrorNtf.size() == propNtf.size()) &&
      (coreGenericErrorNtf[3] == NCI_STATUS_PMU_TXLDO_OVERCURRENT)) {
    coreGenericErrorNtf.assign(propNtf.begin(), propNtf.end());
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "NciStateMonitor %s CORE_GENERIC_ERROR_NTF updated with "
                   "proprietary NTF",
                   __func__);
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS
NciStateMonitor::processCoreResetNtfReceived(vector<uint8_t> coreResetNtf) {
  constexpr uint16_t NCI_MIN_CORE_RESET_NTF_LEN = 0x0C;
  if (coreResetNtf.size() >= NCI_MIN_CORE_RESET_NTF_LEN) {
    firmwareVersion.clear();
    firmwareVersion.assign(coreResetNtf.end() - 3, coreResetNtf.end());
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Invalid Ntf pkt length %zu",
                   __func__, coreResetNtf.size());
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

NFCSTATUS NciStateMonitor::handleVendorNciRspNtf(uint16_t dataLen,
                                              uint8_t *pData) {
  vector<uint8_t> nciRspNtf(pData, pData + dataLen);
  uint16_t mGidOid = ((nciRspNtf[0] << 8) | nciRspNtf[1]);
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;
  constexpr uint16_t NCI_EE_STATUS_NTF =
      (((NCI_MT_NTF | NCI_GID_EE_MANAGE) << 8) | NCI_EE_STATUS_OID);
  constexpr uint16_t NCI_EE_MODE_SET_NTF =
      (((NCI_MT_NTF | NCI_GID_EE_MANAGE) << 8) | NCI_EE_MODE_SET_OID);
  constexpr uint16_t NCI_RF_DISCOVERY_NTF =
      (((NCI_MT_NTF | NCI_GID_RF_MANAGE) << 8) | NCI_RF_DISCOVERY_OID);
  constexpr uint16_t NCI_CORE_RESET_NTF_GID_OID =
      (((NCI_MT_NTF | NCI_GID_CORE) << 8) | NCI_CORE_RESET_OID);
  constexpr uint16_t NCI_CORE_GENERIC_ERROR_NTF =
      (((NCI_MT_NTF | NCI_GID_CORE) << 8) | NCI_CORE_GENERIC_ERROR_OID);

  if (nciRspNtf[NCI_OID_INDEX] == NXP_FLUSH_SRAM_AO_TO_FLASH_OID) {
    mSramCmdRspBuffer.clear();
    mSramCmdRspBuffer.assign(nciRspNtf.begin(), nciRspNtf.end());
    mSramCmdRspCondVar.signal();
    return NFCSTATUS_EXTN_FEATURE_SUCCESS;
  }

  switch (mGidOid) {
  case NCI_EE_STATUS_NTF: {
    status = NfceeStateMonitor::getInstance()->processNfceeStatusNtf(nciRspNtf);
    break;
  }
  case NCI_EE_MODE_SET_NTF: {
    status = NfceeStateMonitor::getInstance()->processNfceeModeSetNtf(nciRspNtf);
    break;
  }
  case NCI_RF_DISCOVERY_NTF: {
    status = RfStateMonitor::getInstance()->processRfIntfActdNtf(nciRspNtf);
    break;
  }
  case NCI_CORE_GENERIC_ERROR_NTF: {
    status = processCoreGenericErrorNtf(nciRspNtf);
    break;
  }
  case NCI_CORE_RESET_NTF_GID_OID: {
    status = processCoreResetNtfReceived(std::move(nciRspNtf));
    break;
  }
  case NCI_RF_DEACTD_NTF_GID_OID:
  case NCI_RF_DEACTD_RSP_GID_OID:
  case NCI_RF_DISC_RSP_GID_OID:
    status = RfStateMonitor::getInstance()->processRfDiscRspNtf(nciRspNtf);
    break;
  default: {
    return status;
  }
  }

  if (nciRspNtf.size() == dataLen) {
    copy(nciRspNtf.begin(), nciRspNtf.end(), pData);
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "NciStateMonitor %s dataLen mismatch", __func__);
  }
  return status;
}

NFCSTATUS NciStateMonitor::processExtnWrite(uint16_t dataLen, const uint8_t *pData) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "NciStateMonitor %s Enter", __func__);
  vector<uint8_t> nciCmd;
  constexpr uint16_t NCI_EE_MODE_SET_CMD_GID_OID =
      (((NCI_MT_CMD | NCI_GID_EE_MANAGE) << 8) | NCI_EE_MODE_SET_OID);
  nciCmd.assign(pData, pData + (dataLen));
  uint16_t mGidOid = ((nciCmd[0] << 8) | nciCmd[1]);
  if ((mGidOid == NCI_EE_MODE_SET_CMD_GID_OID) &&
      (nciCmd.size() > NCI_MODE_SET_CMD_EE_INDEX)) {
    NfceeStateMonitor::getInstance()->setCurrentEE(
        nciCmd[NCI_MODE_SET_CMD_EE_INDEX]);
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

void NciStateMonitor::sendSramConfigFlashCmd() {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "NciStateMonitor %s Enter",
    __func__);
  if (sSendSramConfigToFlash) {
    // Flush SRAM content to flash
    vector<uint8_t> sendSramFlashCmd = {NCI_PROP_CMD_VAL,
                                        NXP_FLUSH_SRAM_AO_TO_FLASH_OID, 0x00};
    mSramCmdRspCondVar.lock();
    PlatformAbstractionLayer::getInstance()->palenQueueWrite(
        sendSramFlashCmd.data(), sendSramFlashCmd.size());
    mSramCmdRspBuffer.clear();
    mSramCmdRspCondVar.timedWait(SEND_SRAM_CMD_TIMEOUT_SEC);
    mSramCmdRspCondVar.unlock();
    if (mSramCmdRspBuffer.size() <= EXT_NFC_STATUS_INDEX ||
        mSramCmdRspBuffer[EXT_NFC_STATUS_INDEX] != EXT_NFC_STATUS_OK) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "sendSramConfigFlashCmd Failed!");
    }
    // Clear the flag so that subsequent pre-discover will not send
    // sram config to flash.
    sSendSramConfigToFlash = false;
  } else {
    NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "sendSramConfigFlashCmd Not Required!");
  }
}

NFCSTATUS NciStateMonitor::handleHalEvent(uint8_t event) {
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "NciStateMonitor %s Enter",
                 __func__);
  if (event == EXT_NFC_PRE_DISCOVER) {
    sendSramConfigFlashCmd();
    return NFCSTATUS_EXTN_FEATURE_FAILURE;
  }
  NFCSTATUS status = NFCSTATUS_EXTN_FEATURE_FAILURE;
  switch (event) {
    case EXT_HAL_NFC_OPEN_CPLT_EVT:
      sSendSramConfigToFlash = true;
      break;
    default:
      break;
  }
  return status;
}
