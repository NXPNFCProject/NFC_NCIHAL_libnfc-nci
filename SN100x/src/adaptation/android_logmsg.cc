/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <log/log.h>

#include "android_logmsg.h"
#include "buildcfg.h"

using android::base::StringPrintf;

extern bool nfc_debug_enabled;
#if (NXP_EXTNS == TRUE)
extern char appl_dta_mode_flag;
#endif

#define MAX_NCI_PACKET_SIZE 259
#define BTE_LOG_BUF_SIZE 1024
#define BTE_LOG_MAX_SIZE (BTE_LOG_BUF_SIZE - 12)
#define MAX_LOGCAT_LINE 4096
#define PRINT(s) __android_log_write(ANDROID_LOG_DEBUG, "BrcmNci", s)
static char log_line[MAX_LOGCAT_LINE];
static const char* sTable = "0123456789abcdef";

static void ToHex(const uint8_t* data, uint16_t len, char* hexString,
                  uint16_t hexStringSize);

void ProtoDispAdapterDisplayNciPacket(uint8_t* nciPacket, uint16_t nciPacketLen,
                                      bool is_recv) {
  if (!nfc_debug_enabled) return;

  char line_buf[(MAX_NCI_PACKET_SIZE * 2) + 1];
  ToHex(nciPacket, nciPacketLen, line_buf, sizeof(line_buf));
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s:%s", is_recv ? "NciR" : "NciX", line_buf);
}

void ToHex(const uint8_t* data, uint16_t len, char* hexString,
           uint16_t hexStringSize) {
  int i = 0, j = 0;
  for (i = 0, j = 0; i < len && j < hexStringSize - 3; i++) {
    hexString[j++] = sTable[(*data >> 4) & 0xf];
    hexString[j++] = sTable[*data & 0xf];
    data++;
  }
  hexString[j] = '\0';
}

inline void byte2hex(const char* data, char** str) {
  **str = sTable[(*data >> 4) & 0xf];
  ++*str;
  **str = sTable[*data & 0xf];
  ++*str;
}

/***************************************************************************
**
** Function         DispLLCP
**
** Description      Log LLCP packet as hex-ascii bytes.
**
** Returns          None.
**
***************************************************************************/
void DispLLCP(NFC_HDR* p_buf, bool is_recv) {
  if (!nfc_debug_enabled) return;

  uint32_t nBytes = ((NFC_HDR_SIZE + p_buf->offset + p_buf->len) * 2) + 1;
  if (nBytes > sizeof(log_line)) return;

  uint8_t* data = (uint8_t*)p_buf;
  int data_len = NFC_HDR_SIZE + p_buf->offset + p_buf->len;
  ToHex(data, data_len, log_line, sizeof(log_line));
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s:%s", is_recv ? "LlcpR" : "LlcpX", log_line);
}

/***************************************************************************
**
** Function         DispHcp
**
** Description      Log raw HCP packet as hex-ascii bytes
**
** Returns          None.
**
***************************************************************************/
void DispHcp(uint8_t* data, uint16_t len, bool is_recv) {
  if (!nfc_debug_enabled) return;

  uint32_t nBytes = (len * 2) + 1;
  if (nBytes > sizeof(log_line)) return;

  ToHex(data, len, log_line, sizeof(log_line));
  DLOG_IF(INFO, nfc_debug_enabled)
      << StringPrintf("%s:%s", is_recv ? "HcpR" : "HcpX", log_line);
}

/***************************************************************************
**
** Function         initializeGlobalAppDtaMode.
**
** Description      initialize Dta App Mode flag.
**
** Returns          None.
**
***************************************************************************/
void initializeGlobalAppDtaMode() {
  appl_dta_mode_flag = 0x01;
  ALOGD("%s: DTA Enabled", __func__);
}
