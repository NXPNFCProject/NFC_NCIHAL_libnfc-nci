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

#include "LxDebug.h"
#include "NfcExtensionApi.h"
#include "NfcExtensionController.h"
#include "NfcExtensionWriter.h"
#include "PlatformAbstractionLayer.h"
#include <cstring>
#include <phNfcNciConstants.h>
#include <phNxpLog.h>
#include <stdlib.h>
#include <string.h>

LxDebug *LxDebug::instance = nullptr;

LxDebug *LxDebug::getInstance() {
  if (instance == nullptr) {
    instance = new LxDebug();
  }
  return instance;
}

void LxDebug::handleEFDMModeSet(bool mode) { mIsEFDMStarted = mode; }

EfdmStatus_t LxDebug::isFieldDetectEnabledFromConfig() {
  uint8_t extended_field_mode = 0x00;
  const uint8_t EXTENDED_FIELD_TAG_WITHOUT_CMA_ENABLE = 0x01;
  const uint8_t EXTENDED_FIELD_TAG_WITH_CMA_ENABLE = 0x03;
  if (GetNxpNumValue(NAME_NXP_EXTENDED_FIELD_DETECT_MODE, &extended_field_mode,
                     sizeof(extended_field_mode))) {
    if (extended_field_mode == EXTENDED_FIELD_TAG_WITHOUT_CMA_ENABLE ||
        extended_field_mode == EXTENDED_FIELD_TAG_WITH_CMA_ENABLE) {
      return EFDM_ENABLED;
    } else {
      return EFDM_DISABLED_CONFIG;
    }
  }
  return EFDM_DISABLED;
}

bool LxDebug::isEFDMStarted() { return mIsEFDMStarted; }

NFCSTATUS LxDebug::processRfDiscCmd(vector<uint8_t> &rfDiscCmd) {
  uint8_t NCI_EFDM_PAYLOAD_LEN = 2;
  uint8_t NCI_RF_DISC_PAYLOAD_LEN_INDEX = 2;
  uint8_t NCI_RF_DISC_NUM_OF_CONFIG_INDEX = 3;
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "LxDebug %s Entry ", __func__);

  if (rfDiscCmd.size() > 1) {
    uint16_t gidOid = ((rfDiscCmd[0] << 8) | rfDiscCmd[1]);
    if (gidOid == NCI_RF_DISC_CMD_GID_OID) {
      /* consider listen is already removed from the command */
      rfDiscCmd[NCI_RF_DISC_PAYLOAD_LEN_INDEX] += NCI_EFDM_PAYLOAD_LEN;
      rfDiscCmd[NCI_RF_DISC_NUM_OF_CONFIG_INDEX]++;
      rfDiscCmd.push_back(0xFF);
      rfDiscCmd.push_back(0x01);
      return NFCSTATUS_EXTN_FEATURE_SUCCESS;
    } else if ((gidOid == NCI_RF_SET_CONFIG_CMD_GID_OID) &&
               updateRFSetConfig(rfDiscCmd)) {
      return NFCSTATUS_EXTN_FEATURE_SUCCESS;
    }
  }
  return NFCSTATUS_EXTN_FEATURE_FAILURE;
}

bool LxDebug::updateRFSetConfig(vector<uint8_t> &rfSetConfCmd) {

  for (size_t i = NCI_PAYLOAD_LEN_INDEX; i < rfSetConfCmd.size() - 2; i++) {
    if (rfSetConfCmd[i] == NCI_PARAM_ID_RF_FIELD_INFO) {
      /* check if length is 1 to confirm the TAG */
      if (rfSetConfCmd[i + 1] == 1) {
        rfSetConfCmd[i + 2] = EFDM_ENABLED;
        return true;
      }
    }
  }
  return false;
}
