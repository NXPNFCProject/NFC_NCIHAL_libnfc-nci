/******************************************************************************
 *
 *  Copyright (C) 2012 Broadcom Corporation
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

/******************************************************************************
 *
 *  Override the Android logging macro(s) from
 *  /system/core/include/cutils/log.h. This header must be the first header
 *  included by a *.cpp file so the original Android macro can be replaced.
 *  Do not include this header in another header, because that will create
 *  unnecessary dependency.
 *
 ******************************************************************************/
#pragma once

#include <android-base/stringprintf.h>
#include <base/logging.h>
#include "bt_types.h"


using android::base::StringPrintf;

extern bool nfc_debug_enabled;
extern uint32_t ScrProtocolTraceFlag;
/* defined for run time DTA mode selection */
extern unsigned char appl_dta_mode_flag;

void initializeGlobalAppDtaMode();

/*******************************************************************************
**
** Function:        initializeGlobalAppLogLevel
**
** Description:     Initialize and get global logging level from .conf or
**                  Android property nfc.app_log_level.  The Android property
**                  overrides .conf variable.
**
** Returns:         Global log level:
**                  0 * No trace messages to be generated
**                  1 * Debug messages
**
*******************************************************************************/
unsigned char initializeGlobalAppLogLevel();
uint32_t initializeProtocolLogLevel();

#if (NXP_EXTNS == TRUE)
/*******************************************************************************
**
** Function:        enableDisableAppLevel
**
** Description:      This function can be used to enable/disable application
**                   trace  logs
**
** Returns:         none:
**
*******************************************************************************/
void enableDisableAppLevel(uint8_t type);
#endif

