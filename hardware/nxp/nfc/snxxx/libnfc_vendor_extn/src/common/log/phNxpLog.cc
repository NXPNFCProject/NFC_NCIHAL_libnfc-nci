/*
 * Copyright 2010-2019, 2022-2025 NXP
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
#define LOG_TAG "NxpNfcHal"
#include "phNxpLog.h"
#include <stdio.h>
#include <string.h>
#include <log/log.h>

const char *NXPLOG_ITEM_EXTNS = "NxpExtns";

/* global log level structure */
nci_log_level_t gLog_level = {4, 4, 4, 4, 4, 4, 4};

bool nfc_debug_enabled = true;
/******************************************************************************
 * Function         phNxpExtLog_EnableDisableLogLevel
 *
 * Description      This function can be called to enable/disable the log levels
 *
 *
 *                  Log Level values:
 *                      NXPLOG_LOG_SILENT_LOGLEVEL  0        * No trace to show
 *                      NXPLOG_LOG_ERROR_LOGLEVEL   1        * Show Error trace
 *only
 *                      NXPLOG_LOG_WARN_LOGLEVEL    2        * Show Warning
 *trace and Error trace
 *                      NXPLOG_LOG_DEBUG_LOGLEVEL   3        * Show all traces
 *
 * Returns          void
 *
 ******************************************************************************/
uint8_t phNxpExtLog_EnableDisableLogLevel(uint8_t enable) {
  static nci_log_level_t prevTraceLevel = {0, 0, 0, 0, 0, 0, 0};
  static uint8_t currState = 0x01;
  static bool prev_debug_enabled = true;
  uint8_t status = NFCSTATUS_FAILED;

  if (0x01 == enable && currState != 0x01) {
    memcpy(&gLog_level, &prevTraceLevel, sizeof(nci_log_level_t));
    nfc_debug_enabled = prev_debug_enabled;
    currState = 0x01;
    status = NFCSTATUS_SUCCESS;
  } else if (0x00 == enable && currState != 0x00) {
    prev_debug_enabled = nfc_debug_enabled;
    memcpy(&prevTraceLevel, &gLog_level, sizeof(nci_log_level_t));
    gLog_level.hal_log_level = 0;
    gLog_level.extns_log_level = 0;
    gLog_level.tml_log_level = 0;
    gLog_level.ncix_log_level = 0;
    gLog_level.ncir_log_level = 0;
    nfc_debug_enabled = false;
    currState = 0x00;
    status = NFCSTATUS_SUCCESS;
  }

  return status;
}
