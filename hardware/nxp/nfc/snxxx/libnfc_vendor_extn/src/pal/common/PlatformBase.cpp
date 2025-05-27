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

#include <thread>
#include "NfcExtensionConstants.h"
#include "PlatformBase.h"
#include <cutils/properties.h>
#include <string>

tNFC_chipType chipType = DEFAULT_CHIP_TYPE;
string product[15] = {"UNKNOWN", "PN547C2", "PN65T", "PN548C2", "PN66T",
                             "PN551",   "PN67T",   "PN553", "PN80T",   "PN557",
                             "PN81T",   "sn100",   "SN220", "pn560",   "SN300"};

static tNFC_chipType determineChipType(uint8_t *msg,
                                                 uint16_t msg_len) {
  tNFC_chipType chip_type = pn81T;
  if ((msg == NULL) || (msg_len < 3)) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Wrong msg_len. Setting Default ChipType pn81T",
                   __func__);
    return chip_type;
  }
  if (GET_FW_ROM_VERSION_NCI_RESP(msg, msg_len) ==
          FW_MOBILE_ROM_VERSION_PN557 &&
      (GET_FW_MAJOR_VERSION_NCI_RESP(msg, msg_len) ==
           FW_MOBILE_MAJOR_NUMBER_PN557 ||
       GET_FW_MAJOR_VERSION_NCI_RESP(msg, msg_len) ==
           FW_MOBILE_MAJOR_NUMBER_PN557_V2))
    chip_type = pn557;
  else if (GET_FW_ROM_VERSION_NCI_RESP(msg, msg_len) ==
               FW_MOBILE_ROM_VERSION_PN553 &&
           GET_FW_MAJOR_VERSION_NCI_RESP(msg, msg_len) ==
               FW_MOBILE_MAJOR_NUMBER_PN553)
    chip_type = pn553;
  else if (GET_FW_ROM_VERSION_NCI_RESP(msg, msg_len) ==
               FW_MOBILE_ROM_VERSION_SN100U &&
           GET_FW_MAJOR_VERSION_NCI_RESP(msg, msg_len) ==
               FW_MOBILE_MAJOR_NUMBER_SN100U)
    chip_type = sn100u;
  /* PN560 & SN220 have same rom & fw major version but chip type is different*/
  else if (GET_FW_ROM_VERSION_NCI_RESP(msg, msg_len) ==
               FW_MOBILE_ROM_VERSION_SN220U &&
           GET_FW_MAJOR_VERSION_NCI_RESP(msg, msg_len) ==
               FW_MOBILE_MAJOR_NUMBER_SN220U) {
    if ((msg_len >= 4) &&
        (GET_HW_VERSION_NCI_RESP(msg, msg_len) == HW_PN560_V1 ||
         GET_HW_VERSION_NCI_RESP(msg, msg_len) == HW_PN560_V2)) {
      chip_type = pn560;
    } else {
      chip_type = sn220u;
    }
  } else if (GET_FW_ROM_VERSION_NCI_RESP(msg, msg_len) ==
                 FW_MOBILE_ROM_VERSION_SN300U &&
             GET_FW_MAJOR_VERSION_NCI_RESP(msg, msg_len) ==
                 FW_MOBILE_MAJOR_NUMBER_SN300U) {
    chip_type = sn300u;
  }
  return chip_type;
}

void PlatformBase::palMemcpy(void *pDest, size_t destSize, const void *pSrc,
                             size_t srcSize) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter srcSize:%zu, destSize:%zu",
                 __func__, srcSize, destSize);
  if (srcSize > destSize) {
    srcSize = destSize; // Truncate to avoid over flow
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                   "%s Truncated length to avoid over flow ", __func__);
  }
  memcpy(pDest, pSrc, srcSize);
}

int PlatformBase::palMemcmp(const void *pSrc1, const void *pSrc2,
                            size_t dataSize) {
  return memcmp(pSrc1, pSrc2, dataSize);
}

void PlatformBase::palThreadJoin(pthread_t tid) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:enter", __func__);

  if(tid == 0){
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:Thread ID is null", __func__);
    return;
  }

  pthread_join(tid, nullptr);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:exit", __func__);
  return;
}

bool PlatformBase::palRunInThread(void *func(void *), pthread_t *tid) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:enter", __func__);
  pthread_attr_t sPThreadAttr;
  pthread_attr_init(&sPThreadAttr);
  int creation_result =
      pthread_create(tid, &sPThreadAttr, threadStartRoutine,
                     reinterpret_cast<void *>(func));
  if (creation_result >= 0) {
    NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "PalThread creation success!");
  } else {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "PalThread creation failed!");
  }
  pthread_attr_destroy(&sPThreadAttr);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:exit", __func__);
  return creation_result >= 0;
}

void *PlatformBase::threadStartRoutine(void *arg) {
  auto funcPtr = reinterpret_cast<void *(*)(void *)>(arg);
  void *result = funcPtr(nullptr);
  pthread_exit(result);
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s : pthread_exit done", __func__);
  return nullptr;
}

int PlatformBase::palPropertySet(const char *key, const char *value) {
  return property_set(key, value);
}

tNFC_chipType PlatformBase::palGetChipType() {
  return chipType;
}

tNFC_chipType PlatformBase::processChipType(uint8_t* msg,
                                                        uint16_t msg_len) {
  NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s Enter", __func__);
  if ((msg != NULL) && (msg_len != 0)) {
    if (msg[0] == NCI_MT_NTF &&
        ((msg[1] & NCI_OID_MASK) == NCI_MSG_CORE_RESET)) {
      if ((msg_len > 2) && msg[0] == NCI_CMD_RSP_SUCCESS_SW1 &&
          msg[1] == NCI_CMD_RSP_SUCCESS_SW2) {
        chipType = determineChipType(msg, msg_len);
        NXPLOG_EXTNS_D(NXPLOG_ITEM_NXP_GEN_EXTN, "%s ChipType: %s", __func__,
                       product[chipType].c_str());
      } else {
        NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                       "%s Wrong msg_len. Setting Default ChipType pn80T",
                       __func__);
        chipType = pn81T;
      }
    }
  }
  return chipType;
}
