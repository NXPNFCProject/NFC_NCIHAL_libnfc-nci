/*
 * Copyright 2025 NXP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Type4Tag.h"
#include "phNxpLog.h"

#define NXP_NDEF_TAG_EMULATION_LOGICAL_CHANNEL 5

Type4Tag *Type4Tag::sInstance = nullptr;

Type4Tag::Type4Tag() {
    cmdType4TagNfceeCfg = 0;
    mEepromInfo = {.request_mode = 0};
}

Type4Tag::~Type4Tag() {

}

Type4Tag *Type4Tag::getInstance() {
    if(sInstance == nullptr) {
        sInstance = new Type4Tag();
    }
    return sInstance;
}

NFCSTATUS Type4Tag::enable() {
  long retlen = 0;
  if (!GetNxpNumValue(NAME_T4T_NFCEE_ENABLE, (void*)&retlen,
      sizeof(retlen))) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
      "%s T4T_NFCEE_ENABLE not found. Set to default value:%lX", __func__,
       retlen);
  }
  cmdType4TagNfceeCfg = (uint8_t)retlen;
  mEepromInfo.buffer = &cmdType4TagNfceeCfg;
  mEepromInfo.bufflen = sizeof(cmdType4TagNfceeCfg);
  mEepromInfo.request_type = EEPROM_T4T_NFCEE_ENABLE;
  mEepromInfo.request_mode = SET_EEPROM_DATA;
  return request_EEPROM(&mEepromInfo);
}

NFCSTATUS Type4Tag::updatePropParam(uint16_t dataLen, uint8_t *pData) {
  uint8_t retlen = 0;
  if (GetNxpNumValue(NAME_T4T_NFCEE_ENABLE, (void*)&retlen, sizeof(retlen))) {
    if (retlen > 0 && dataLen > 8) {
      /* If NDEF T4T is enabled, change Max Logical Connections to 5. FW will
          return 0x01 By default. But nfc_alloc_conn_cb will fail this way */
      pData[8] = NXP_NDEF_TAG_EMULATION_LOGICAL_CHANNEL;
      NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
          "%s Updated number of logical channels:%lX", __func__,pData[8]);
    } else {
      // No action is needed, use default value
    }
  }
  return NFCSTATUS_SUCCESS;
}