/*
 * Copyright 2010-2014, 2022-2025 NXP
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

#if !defined(NXPLOG__H_INCLUDED)
#define NXPLOG__H_INCLUDED
#include <log/log.h>
#include <phNfcStatus.h>

typedef struct nci_log_level {
  uint8_t global_log_level;
  uint8_t extns_log_level;
  uint8_t hal_log_level;
  uint8_t dnld_log_level;
  uint8_t tml_log_level;
  uint8_t ncix_log_level;
  uint8_t ncir_log_level;
} nci_log_level_t;

/* global log level Ref */
extern nci_log_level_t gLog_level;
extern bool nfc_debug_enabled;

/* define log module included when compile */
#define ENABLE_EXTNS_TRACES TRUE

/* ####################### Set the logging level for EVERY COMPONENT here
 * ######################## :START: */
#define NXPLOG_LOG_SILENT_LOGLEVEL 0x00
#define NXPLOG_LOG_ERROR_LOGLEVEL 0x01
#define NXPLOG_LOG_WARN_LOGLEVEL 0x02
#define NXPLOG_LOG_INFO_LOGLEVEL 0x03
#define NXPLOG_LOG_DEBUG_LOGLEVEL 0x04
/* ####################### Set the default logging level for EVERY COMPONENT
 * here ########################## :END: */

/* The Default log level for all the modules. */
#define NXPLOG_DEFAULT_LOGLEVEL NXPLOG_LOG_ERROR_LOGLEVEL

extern const char *NXPLOG_ITEM_EXTNS;  /* Android logging tag for NxpExtns  */
/**
 * @brief Log name of the Gen extension library
 *
 */
static const char *NXPLOG_ITEM_NXP_GEN_EXTN = "NxpGenExtn";

/* ######################################## Defines used for Logging data
 * ######################################### */
#ifdef NXP_VRBS_REQ
#define NXPLOG_FUNC_ENTRY(COMP)                                                \
  LOG_PRI(ANDROID_LOG_VERBOSE, (COMP), "+:%s", (__func__))
#define NXPLOG_FUNC_EXIT(COMP)                                                 \
  LOG_PRI(ANDROID_LOG_VERBOSE, (COMP), "-:%s", (__func__))
#endif /*NXP_VRBS_REQ*/

/* ################################################################################################################
 */
/* ######################################## Logging APIs of actual modules
 * ######################################## */
/* ################################################################################################################
 */
/* Logging APIs used by NxpExtns module */
#if (ENABLE_EXTNS_TRACES == TRUE)
#define NXPLOG_EXTNS_D(COMP, ...)                                              \
  {                                                                            \
    if ((nfc_debug_enabled) ||                                                 \
        (gLog_level.extns_log_level >= NXPLOG_LOG_DEBUG_LOGLEVEL))             \
      LOG_PRI(ANDROID_LOG_DEBUG, COMP, __VA_ARGS__);                           \
  }
#define NXPLOG_EXTNS_I(COMP, ...)                                              \
  {                                                                            \
    if ((nfc_debug_enabled) ||                                                 \
        (gLog_level.extns_log_level >= NXPLOG_LOG_INFO_LOGLEVEL))              \
      LOG_PRI(ANDROID_LOG_INFO, COMP, __VA_ARGS__);                            \
  }
#define NXPLOG_EXTNS_W(COMP, ...)                                              \
  {                                                                            \
    if ((nfc_debug_enabled) ||                                                 \
        (gLog_level.extns_log_level >= NXPLOG_LOG_WARN_LOGLEVEL))              \
      LOG_PRI(ANDROID_LOG_WARN, COMP, __VA_ARGS__);                            \
  }
#define NXPLOG_EXTNS_E(COMP, ...)                                              \
  {                                                                            \
    if (gLog_level.extns_log_level >= NXPLOG_LOG_ERROR_LOGLEVEL)               \
      LOG_PRI(ANDROID_LOG_ERROR, COMP, __VA_ARGS__);                           \
  }
#else
#define NXPLOG_EXTNS_D(...)
#define NXPLOG_EXTNS_W(...)
#define NXPLOG_EXTNS_E(...)
#define NXPLOG_EXTNS_I(...)
#endif /* Logging APIs used by NxpExtns module */

#ifdef NXP_VRBS_REQ
#if (ENABLE_EXTNS_TRACES == TRUE)
#define NXPLOG_EXTNS_ENTRY() NXPLOG_FUNC_ENTRY(NXPLOG_ITEM_EXTNS)
#define NXPLOG_EXTNS_EXIT() NXPLOG_FUNC_EXIT(NXPLOG_ITEM_EXTNS)
#else
#define NXPLOG_EXTNS_ENTRY()
#define NXPLOG_EXTNS_EXIT()
#endif
#endif /* NXP_VRBS_REQ */

uint8_t phNxpExtLog_EnableDisableLogLevel(uint8_t enable);

#endif /* NXPLOG__H_INCLUDED */
