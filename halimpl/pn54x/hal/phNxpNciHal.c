/*
 * Copyright (C) 2015 NXP Semiconductors
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

#include <sys/stat.h>
#include <phNxpNciHal.h>
#include <phNxpNciHal_ext.h>
#include <phNxpNciHal_Dnld.h>
#include <phNxpNciHal_Adaptation.h>
#include <phTmlNfc.h>
#include <phDnldNfc.h>
#include <phDal4Nfc_messageQueueLib.h>
#include <phNxpLog.h>
#include <phNxpConfig.h>
#include <phNxpNciHal_NfcDepSWPrio.h>
#include <phNxpNciHal_Kovio.h>
#include <phTmlNfc_i2c.h>
/*********************** Global Variables *************************************/
#define PN547C2_CLOCK_SETTING
#undef PN547C2_FACTORY_RESET_DEBUG
#define CORE_RES_STATUS_BYTE 3
#if (NXP_NFCC_FORCE_NCI1_0_INIT == true)
#ifndef PH_LIBNFC_ENABLE_FORCE_DOWNLOAD
#define PH_LIBNFC_ENABLE_FORCE_DOWNLOAD false
#endif
#endif


/* Processing of ISO 15693 EOF */
extern uint8_t icode_send_eof;
extern uint8_t icode_detected;
static uint8_t cmd_icode_eof[] = {0x00, 0x00, 0x00};
static uint8_t read_failed_disable_nfc = false;
static uint8_t pwr_link_required = false;
static uint8_t config_access = false;
static uint8_t config_success = true;
static NFCSTATUS phNxpNciHal_FwDwnld(void);
static NFCSTATUS phNxpNciHal_SendCmd(uint8_t cmd_len, uint8_t* pcmd_buff);
static void phNxpNciHal_check_delete_nfaStorage_DHArea();
/* NCI HAL Control structure */
phNxpNciHal_Control_t nxpncihal_ctrl;

/* NXP Poll Profile structure */
phNxpNciProfile_Control_t nxpprofile_ctrl;

/* TML Context */
extern phTmlNfc_Context_t* gpphTmlNfc_Context;
extern void phTmlNfc_set_fragmentation_enabled(
    phTmlNfc_i2cfragmentation_t result);

extern int phNxpNciHal_CheckFwRegFlashRequired(uint8_t* fw_update_req,
                                               uint8_t* rf_update_req);
phNxpNci_getCfg_info_t* mGetCfg_info = NULL;
uint32_t gSvddSyncOff_Delay = 10;
bool_t force_fw_download_req = false;
/* global variable to get FW version from NCI response*/
uint32_t wFwVerRsp;
/* External global variable to get FW version */
extern uint16_t wFwVer;

extern uint16_t fw_maj_ver;
extern uint16_t rom_version;
extern int send_to_upper_kovio;
extern int kovio_detected;
extern int disable_kovio;
extern uint8_t
    gRecFWDwnld; /* flag  set to true to  indicate dummy FW download */
static uint8_t gRecFwRetryCount;  // variable to hold dummy FW recovery count
static uint8_t write_unlocked_status = NFCSTATUS_SUCCESS;
static uint8_t Rx_data[NCI_MAX_DATA_LEN];
uint32_t timeoutTimerId = 0;
#ifndef FW_DWNLD_FLAG
uint8_t fw_dwnld_flag = false;
#endif
phNxpNciHal_Sem_t config_data;

phNxpNciClock_t phNxpNciClock = {0, {0}, false};

phNxpNciRfSetting_t phNxpNciRfSet = {false, {0}};

/**************** local methods used in this file only ************************/
static NFCSTATUS phNxpNciHal_fw_download(void);
static void phNxpNciHal_open_complete(NFCSTATUS status);
static void phNxpNciHal_write_complete(void* pContext,
                                       phTmlNfc_TransactInfo_t* pInfo);
static void phNxpNciHal_read_complete(void* pContext,
                                      phTmlNfc_TransactInfo_t* pInfo);
static void phNxpNciHal_close_complete(NFCSTATUS status);
static void phNxpNciHal_core_initialized_complete(NFCSTATUS status);
static void phNxpNciHal_pre_discover_complete(NFCSTATUS status);
static void phNxpNciHal_power_cycle_complete(NFCSTATUS status);
static void phNxpNciHal_kill_client_thread(
    phNxpNciHal_Control_t* p_nxpncihal_ctrl);
static void* phNxpNciHal_client_thread(void* arg);
static void phNxpNciHal_get_clk_freq(void);
static void phNxpNciHal_set_clock(void);
static void phNxpNciHal_check_factory_reset(void);
static NFCSTATUS phNxpNciHal_check_eSE_Session_Identity(void);
static void phNxpNciHal_print_res_status(uint8_t* p_rx_data, uint16_t* p_len);
static NFCSTATUS phNxpNciHal_CheckValidFwVersion(void);
static void phNxpNciHal_enable_i2c_fragmentation();
static void phNxpNciHal_core_MinInitialized_complete(NFCSTATUS status);
static NFCSTATUS phNxpNciHal_set_Boot_Mode(uint8_t mode);
NFCSTATUS phNxpNciHal_check_clock_config(void);
NFCSTATUS phNxpNciHal_set_china_region_configs(void);
static NFCSTATUS phNxpNciHalRFConfigCmdRecSequence();
static NFCSTATUS phNxpNciHal_CheckRFCmdRespStatus();
int check_config_parameter();
static NFCSTATUS phNxpNciHal_uicc_baud_rate();

//static tNfc_featureList phNxpNciHal_getFeatureList();
/******************************************************************************
 * Function         phNxpNciHal_client_thread
 *
 * Description      This function is a thread handler which handles all TML and
 *                  NCI messages.
 *
 * Returns          void
 *
 ******************************************************************************/
static void* phNxpNciHal_client_thread(void* arg) {
  phNxpNciHal_Control_t* p_nxpncihal_ctrl = (phNxpNciHal_Control_t*)arg;
  phLibNfc_Message_t msg;

  NXPLOG_NCIHAL_D("thread started");

  p_nxpncihal_ctrl->thread_running = 1;

  while (p_nxpncihal_ctrl->thread_running == 1) {
    /* Fetch next message from the NFC stack message queue */
    if (phDal4Nfc_msgrcv(p_nxpncihal_ctrl->gDrvCfg.nClientId, &msg, 0, 0) ==
        -1) {
      NXPLOG_NCIHAL_E("NFC client received bad message");
      continue;
    }

    if (p_nxpncihal_ctrl->thread_running == 0) {
      break;
    }

    switch (msg.eMsgType) {
      case PH_LIBNFC_DEFERREDCALL_MSG: {
        phLibNfc_DeferredCall_t* deferCall =
            (phLibNfc_DeferredCall_t*)(msg.pMsgData);

        REENTRANCE_LOCK();
        deferCall->pCallback(deferCall->pParameter);
        REENTRANCE_UNLOCK();

        break;
      }

      case NCI_HAL_OPEN_CPLT_MSG: {
        REENTRANCE_LOCK();
        if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
          /* Send the event */
          (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_OPEN_CPLT_EVT,
                                              HAL_NFC_STATUS_OK);
        }
        REENTRANCE_UNLOCK();
        break;
      }

      case NCI_HAL_CLOSE_CPLT_MSG: {
        REENTRANCE_LOCK();
        if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
          /* Send the event */
          (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_CLOSE_CPLT_EVT,
                                              HAL_NFC_STATUS_OK);
          phNxpNciHal_kill_client_thread(&nxpncihal_ctrl);
        }
        REENTRANCE_UNLOCK();
        break;
      }

      case NCI_HAL_POST_INIT_CPLT_MSG: {
        REENTRANCE_LOCK();
        if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
          /* Send the event */
          (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_POST_INIT_CPLT_EVT,
                                              HAL_NFC_STATUS_OK);
        }
        REENTRANCE_UNLOCK();
        break;
      }

      case NCI_HAL_PRE_DISCOVER_CPLT_MSG: {
        REENTRANCE_LOCK();
        if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
          /* Send the event */
          (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_PRE_DISCOVER_CPLT_EVT,
                                              HAL_NFC_STATUS_OK);
        }
        REENTRANCE_UNLOCK();
        break;
      }

      case NCI_HAL_ERROR_MSG: {
        REENTRANCE_LOCK();
        if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
          /* Send the event */
          (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_ERROR_EVT,
                                              HAL_NFC_STATUS_FAILED);
        }
        REENTRANCE_UNLOCK();
        break;
      }

      case NCI_HAL_RX_MSG: {
        REENTRANCE_LOCK();
        if (nxpncihal_ctrl.p_nfc_stack_data_cback != NULL) {
          (*nxpncihal_ctrl.p_nfc_stack_data_cback)(nxpncihal_ctrl.rsp_len,
                                                   nxpncihal_ctrl.p_rsp_data);
        }
        REENTRANCE_UNLOCK();
        break;
      }
      case NCI_HAL_POST_MIN_INIT_CPLT_MSG: {
        REENTRANCE_LOCK();
        if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
          /* Send the event */
          (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_POST_MIN_INIT_CPLT_EVT,
                                              HAL_NFC_STATUS_OK);
        }
        REENTRANCE_UNLOCK();
        break;
      }
    }
  }

  NXPLOG_NCIHAL_D("NxpNciHal thread stopped");

  pthread_exit(NULL);
  return NULL;
}

/******************************************************************************
 * Function         phNxpNciHal_kill_client_thread
 *
 * Description      This function safely kill the client thread and clean all
 *                  resources.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_kill_client_thread(
    phNxpNciHal_Control_t* p_nxpncihal_ctrl) {
  NXPLOG_NCIHAL_D("Terminating phNxpNciHal client thread...");

  p_nxpncihal_ctrl->p_nfc_stack_cback = NULL;
  p_nxpncihal_ctrl->p_nfc_stack_data_cback = NULL;
  p_nxpncihal_ctrl->thread_running = 0;

  return;
}

/******************************************************************************
 * Function         phNxpNciHal_fw_download
 *
 * Description      This function download the PN54X secure firmware to IC. If
 *                  firmware version in Android filesystem and firmware in the
 *                  IC is same then firmware download will return with success
 *                  without downloading the firmware.
 *
 * Returns          NFCSTATUS_SUCCESS if firmware download successful
 *                  NFCSTATUS_FAILED in case of failure
 *
 ******************************************************************************/
static NFCSTATUS phNxpNciHal_fw_download(void) {
  NFCSTATUS status = NFCSTATUS_FAILED;
  NFCSTATUS wConfigStatus = NFCSTATUS_FAILED;
  phNxpNciHal_get_clk_freq();
  status = phTmlNfc_IoCtl(phTmlNfc_e_EnableDownloadMode);
  if (NFCSTATUS_SUCCESS == status) {
    /* Set the obtained device handle to download module */
    phDnldNfc_SetHwDevHandle();
    NXPLOG_NCIHAL_D("Calling Seq handler for FW Download \n");
if(nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD) {
    status = phNxpNciHal_fw_download_seq(nxpprofile_ctrl.bClkSrcVal,
                                         nxpprofile_ctrl.bClkFreqVal,
                                         force_fw_download_req);
}
else {
    status = phNxpNciHal_fw_download_seq(nxpprofile_ctrl.bClkSrcVal,
                                         nxpprofile_ctrl.bClkFreqVal,
                                         false);
}
    phDnldNfc_ReSetHwDevHandle();
    /* call read pending */
    wConfigStatus = phTmlNfc_Read(
        nxpncihal_ctrl.p_cmd_data, NCI_MAX_DATA_LEN,
        (pphTmlNfc_TransactCompletionCb_t)&phNxpNciHal_read_complete, NULL);
    if (wConfigStatus != NFCSTATUS_PENDING) {
      NXPLOG_NCIHAL_E("TML Read status error status = %x", status);
      phTmlNfc_Shutdown();
      status = NFCSTATUS_FAILED;
    }
  } else {
    status = NFCSTATUS_FAILED;
  }

  return status;
}

/******************************************************************************
 * Function         phNxpNciHal_CheckValidFwVersion
 *
 * Description      This function checks the valid FW for Mobile device.
 *                  If the FW doesn't belong the Mobile device it further
 *                  checks nxp config file to override.
 *
 * Returns          NFCSTATUS_SUCCESS if valid fw version found
 *                  NFCSTATUS_NOT_ALLOWED in case of FW not valid for mobile
 *                  device
 *
 ******************************************************************************/
static NFCSTATUS phNxpNciHal_CheckValidFwVersion(void) {
  NFCSTATUS status = NFCSTATUS_NOT_ALLOWED;
  const unsigned char sfw_infra_major_no = 0x02;
  unsigned char ufw_current_major_no = 0x00;
  unsigned long num = 0;
  int isfound = 0;

  /* extract the firmware's major no */
  ufw_current_major_no = ((0x00FF) & (wFwVer >> 8U));

  NXPLOG_NCIHAL_D("%s current_major_no = 0x%x", __func__, ufw_current_major_no);

  if (ufw_current_major_no == nfcFL.nfcMwFL._FW_MOBILE_MAJOR_NUMBER) {
    status = NFCSTATUS_SUCCESS;
  } else if (ufw_current_major_no == sfw_infra_major_no) {
        if(rom_version == FW_MOBILE_ROM_VERSION_PN553 || rom_version == FW_MOBILE_ROM_VERSION_PN557) {
          NXPLOG_NCIHAL_D(" PN557  allow Fw download with major number =  0x%x",
          ufw_current_major_no);
          status = NFCSTATUS_SUCCESS;
        } else {
    /* Check the nxp config file if still want to go for download */
    /* By default NAME_NXP_FW_PROTECION_OVERRIDE will not be defined in config
       file.
       If user really want to override the Infra firmware over mobile firmware,
       please
       put "NXP_FW_PROTECION_OVERRIDE=0x01" in libnfc-nxp.conf file.
       Please note once Infra firmware downloaded to Mobile device, The device
       can never be updated to Mobile firmware*/
    isfound = GetNxpNumValue(NAME_NXP_FW_PROTECION_OVERRIDE, &num, sizeof(num));
    if (isfound > 0) {
      if (num == 0x01) {
        NXPLOG_NCIHAL_D("Override Infra FW over Mobile");
        status = NFCSTATUS_SUCCESS;
      } else {
        NXPLOG_NCIHAL_D(
            "Firmware download not allowed (NXP_FW_PROTECION_OVERRIDE invalid "
            "value)");
      }
    } else {
      NXPLOG_NCIHAL_D(
          "Firmware download not allowed (NXP_FW_PROTECION_OVERRIDE not "
          "defiend)");
    }
  }
  }
  else if ((nfcFL.chipType != pn547C2) && (gRecFWDwnld == true)) {
    status = NFCSTATUS_SUCCESS;
  }
  else if (wFwVerRsp == 0) {
    NXPLOG_NCIHAL_E(
        "FW Version not received by NCI command >>> Force Firmware download");
    status = NFCSTATUS_SUCCESS;
  } else {
    NXPLOG_NCIHAL_E("Wrong FW Version >>> Firmware download not allowed");
  }

  return status;
}

static void phNxpNciHal_get_clk_freq(void) {
  unsigned long num = 0;
  int isfound = 0;

  nxpprofile_ctrl.bClkSrcVal = 0;
  nxpprofile_ctrl.bClkFreqVal = 0;
  nxpprofile_ctrl.bTimeout = 0;

  isfound = GetNxpNumValue(NAME_NXP_SYS_CLK_SRC_SEL, &num, sizeof(num));
  if (isfound > 0) {
    nxpprofile_ctrl.bClkSrcVal = num;
  }

  num = 0;
  isfound = 0;
  isfound = GetNxpNumValue(NAME_NXP_SYS_CLK_FREQ_SEL, &num, sizeof(num));
  if (isfound > 0) {
    nxpprofile_ctrl.bClkFreqVal = num;
  }

  num = 0;
  isfound = 0;
  isfound = GetNxpNumValue(NAME_NXP_SYS_CLOCK_TO_CFG, &num, sizeof(num));
  if (isfound > 0) {
    nxpprofile_ctrl.bTimeout = num;
  }

  NXPLOG_FWDNLD_D("gphNxpNciHal_fw_IoctlCtx.bClkSrcVal = 0x%x",
                  nxpprofile_ctrl.bClkSrcVal);
  NXPLOG_FWDNLD_D("gphNxpNciHal_fw_IoctlCtx.bClkFreqVal = 0x%x",
                  nxpprofile_ctrl.bClkFreqVal);
  NXPLOG_FWDNLD_D("gphNxpNciHal_fw_IoctlCtx.bClkFreqVal = 0x%x",
                  nxpprofile_ctrl.bTimeout);

  if ((nxpprofile_ctrl.bClkSrcVal < CLK_SRC_XTAL) ||
      (nxpprofile_ctrl.bClkSrcVal > CLK_SRC_PLL)) {
    NXPLOG_FWDNLD_E(
        "Clock source value is wrong in config file, setting it as default");
    nxpprofile_ctrl.bClkSrcVal = NXP_SYS_CLK_SRC_SEL;
  }
  if ((nxpprofile_ctrl.bClkFreqVal < CLK_FREQ_13MHZ) ||
      (nxpprofile_ctrl.bClkFreqVal > CLK_FREQ_52MHZ)) {
    NXPLOG_FWDNLD_E(
        "Clock frequency value is wrong in config file, setting it as default");
    nxpprofile_ctrl.bClkFreqVal = NXP_SYS_CLK_FREQ_SEL;
  }
  if ((nxpprofile_ctrl.bTimeout < CLK_TO_CFG_DEF) ||
      (nxpprofile_ctrl.bTimeout > CLK_TO_CFG_MAX)) {
    NXPLOG_FWDNLD_E(
        "Clock timeout value is wrong in config file, setting it as default");
    nxpprofile_ctrl.bTimeout = CLK_TO_CFG_DEF;
  }
}
/******************************************************************************
 * Function         phNxpNciHal_FwDwnld
 *
 * Description      This function is called by libnfc-nci after core init is
 *                  completed, to download the firmware.
 *
 * Returns          This function return NFCSTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
static NFCSTATUS phNxpNciHal_FwDwnld(void) {
  NFCSTATUS status = NFCSTATUS_SUCCESS, wConfigStatus = NFCSTATUS_SUCCESS;
  if (wFwVerRsp == 0) {
    phDnldNfc_InitImgInfo();
  }
  status = phNxpNciHal_CheckValidFwVersion();
  if (NFCSTATUS_SUCCESS == status) {
    NXPLOG_NCIHAL_D("Found Valid Firmware Type");
    status = phNxpNciHal_fw_download();
    if (status != NFCSTATUS_SUCCESS) {
      if (NFCSTATUS_SUCCESS != phNxpNciHal_fw_mw_ver_check()) {
        NXPLOG_NCIHAL_D("Chip Version Middleware Version mismatch!!!!");
        goto clean_and_return;
      }
      NXPLOG_NCIHAL_E("FW Download failed - NFCC init will continue");
    }
  } else {
    if (wFwVerRsp == 0) phDnldNfc_ReSetHwDevHandle();
  }
clean_and_return:
  return status;
}
/******************************************************************************
 * Function         phNxpNciHal_MinOpen
 *
 * Description      This function is called by libnfc-nci during the minimum
 *                  initialization of the NFCC. It opens the physical connection
 *                  with NFCC (PN54X) and creates required client thread for
 *                  operation.
 *                  After open is complete, status is informed to libnfc-nci
 *                  through callback function.
 *
 * Returns          This function return NFCSTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *                  NFCSTATUS_FAILED(0xFF)
 *
 ******************************************************************************/
int phNxpNciHal_MinOpen(nfc_stack_callback_t* p_cback,
                        nfc_stack_data_callback_t* p_data_cback) {
  NXPLOG_NCIHAL_E("Init monitor phNxpNciHal_MinOpen");
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  phOsalNfc_Config_t tOsalConfig;
  phTmlNfc_Config_t tTmlConfig;
  static phLibNfc_Message_t msg;
  int init_retry_cnt = 0;
  /*NCI_RESET_CMD*/
  uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x01};
  /*NCI_INIT_CMD*/
  uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  uint8_t boot_mode = nxpncihal_ctrl.hal_boot_mode;
  char* nfc_dev_node = NULL;
  const uint16_t max_len = 260;

  phNxpLog_InitializeLogLevel();

  /*Create the timer for extns write response*/
  timeoutTimerId = phOsalNfc_Timer_Create();
  if (timeoutTimerId == PH_OSALNFC_TIMER_ID_INVALID) {
    NXPLOG_NCIHAL_E("Invalid Timer Id, Timer Creation failed");
    return NFCSTATUS_FAILED;
  }

  if (phNxpNciHal_init_monitor() == NULL) {
    NXPLOG_NCIHAL_E("Init monitor failed");
    phOsalNfc_Timer_Delete(timeoutTimerId);
    return NFCSTATUS_FAILED;
  }

  CONCURRENCY_LOCK();

  memset(&nxpncihal_ctrl, 0x00, sizeof(nxpncihal_ctrl));
  memset(&tOsalConfig, 0x00, sizeof(tOsalConfig));
  memset(&tTmlConfig, 0x00, sizeof(tTmlConfig));
  memset(&nxpprofile_ctrl, 0, sizeof(phNxpNciProfile_Control_t));

  /* By default HAL status is HAL_STATUS_OPEN */
  nxpncihal_ctrl.halStatus = HAL_STATUS_OPEN;

  nxpncihal_ctrl.p_nfc_stack_cback = p_cback;
  nxpncihal_ctrl.p_nfc_stack_data_cback = p_data_cback;

  /* Configure hardware link */
  nxpncihal_ctrl.gDrvCfg.nClientId = phDal4Nfc_msgget(0, 0600);
  nxpncihal_ctrl.gDrvCfg.nLinkType = ENUM_LINK_TYPE_I2C; /* For PN54X */
  nxpncihal_ctrl.hal_boot_mode = boot_mode;

  /*Get the device node name from config file*/
  /* Read the nfc device node name */
  nfc_dev_node = (char*)nxp_malloc(max_len * sizeof(char));
  if (nfc_dev_node == NULL) {
    NXPLOG_NCIHAL_E("malloc of nfc_dev_node failed ");

    CONCURRENCY_UNLOCK();

    return NFCSTATUS_FAILED;
  } else if (!GetNxpStrValue(NAME_NXP_NFC_DEV_NODE, nfc_dev_node,
                             max_len * sizeof(uint8_t))) {
    NXPLOG_NCIHAL_E(
        "Invalid nfc device node name keeping the default device node "
        "/dev/pn54x");
    strcpy(nfc_dev_node, "/dev/pn54x");
  }

  tTmlConfig.pDevName = (int8_t*)nfc_dev_node;

  tOsalConfig.dwCallbackThreadId = (uintptr_t)nxpncihal_ctrl.gDrvCfg.nClientId;
  tOsalConfig.pLogFile = NULL;
  tTmlConfig.dwGetMsgThreadId = (uintptr_t)nxpncihal_ctrl.gDrvCfg.nClientId;

  /* Initialize TML layer */
  status = phTmlNfc_Init(&tTmlConfig);
  if (status == NFCSTATUS_SUCCESS) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&nxpncihal_ctrl.client_thread, &attr,
                       phNxpNciHal_client_thread, &nxpncihal_ctrl) == 0x00) {
      status = phTmlNfc_Read(
          nxpncihal_ctrl.p_cmd_data, NCI_MAX_DATA_LEN,
          (pphTmlNfc_TransactCompletionCb_t)&phNxpNciHal_read_complete, NULL);
      if (status == NFCSTATUS_PENDING) {
        phNxpNciHal_ext_init();
        do {
          status = phNxpNciHal_SendCmd(sizeof(cmd_reset_nci), cmd_reset_nci);
          if (status == NFCSTATUS_SUCCESS) {
            status = phNxpNciHal_SendCmd(sizeof(cmd_init_nci), cmd_init_nci);
          }
          if (status != NFCSTATUS_SUCCESS) {
            (void)phNxpNciHal_power_cycle();
          } else {
            break;
          }
          init_retry_cnt++;
        } while (init_retry_cnt < 0x03);
      }
    }
  }
  CONCURRENCY_UNLOCK();
  init_retry_cnt = 0;
  if (status == NFCSTATUS_SUCCESS) {
    /* print message*/
    phNxpNciHal_core_MinInitialized_complete(status);
  } else {
    phTmlNfc_Shutdown();
    (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_POST_MIN_INIT_CPLT_EVT,
                                        HAL_NFC_STATUS_FAILED);
    /* Report error status */
    nxpncihal_ctrl.p_nfc_stack_cback = NULL;
    nxpncihal_ctrl.p_nfc_stack_data_cback = NULL;
    phNxpNciHal_cleanup_monitor();
    nxpncihal_ctrl.halStatus = HAL_STATUS_CLOSE;
  }
  if (nfc_dev_node != NULL) {
    free(nfc_dev_node);
    nfc_dev_node = NULL;
  }
  return status;
}

static NFCSTATUS phNxpNciHal_SendCmd(uint8_t cmd_len, uint8_t* pcmd_buff) {
  int counter = 0x00;
  NFCSTATUS status;
  do {
    status = NFCSTATUS_FAILED;
    status = phNxpNciHal_send_ext_cmd(cmd_len, pcmd_buff);
    counter++;
  } while (counter < 0x03 && status != NFCSTATUS_SUCCESS);
  return status;
}
/******************************************************************************
 * Function         phNxpNciHal_open
 *
 * Description      This function is called by libnfc-nci during the
 *                  initialization of the NFCC. It opens the physical connection
 *                  with NFCC (PN54X) and creates required client thread for
 *                  operation.
 *                  After open is complete, status is informed to libnfc-nci
 *                  through callback function.
 *
 * Returns          This function return NFCSTATUS_SUCCES (0) in case of success
 *                  In case of failure returns other failure value.
 *
 ******************************************************************************/
int phNxpNciHal_open(nfc_stack_callback_t* p_cback,
                     nfc_stack_data_callback_t* p_data_cback) {
  NFCSTATUS wConfigStatus = NFCSTATUS_SUCCESS;
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  uint8_t boot_mode = nxpncihal_ctrl.hal_boot_mode;
  uint8_t* nfc_dev_node = NULL;
  uint8_t ret_val = 0x00;
  const uint16_t max_len =
      260; /* device node name is max of 255 bytes + 5 bytes (/dev/) */
  static uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  static uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  /*NCI2_0_INIT_CMD*/
  static uint8_t cmd_init_nci2_0[] = {0x20,0x01,0x02,0x00,0x00};
  phTmlNfc_Config_t tTmlConfig;
  phOsalNfc_Config_t tOsalConfig;
  int init_retry_cnt = 0;

  if (nxpncihal_ctrl.hal_boot_mode == NFC_FAST_BOOT_MODE) {
    NXPLOG_NCIHAL_E(" HAL NFC fast init mode calling min_open %d",
                    nxpncihal_ctrl.hal_boot_mode);
    wConfigStatus = phNxpNciHal_MinOpen(p_cback, p_data_cback);
    return wConfigStatus;
  }

  if (nxpncihal_ctrl.halStatus == HAL_STATUS_OPEN) {
    NXPLOG_NCIHAL_E("phNxpNciHal_open already open");
    return NFCSTATUS_SUCCESS;
  }
  /* reset config cache */
  resetNxpConfig();
  phNxpLog_InitializeLogLevel();

  /*Create the timer for extns write response*/
  timeoutTimerId = phOsalNfc_Timer_Create();
  if (timeoutTimerId == PH_OSALNFC_TIMER_ID_INVALID) {
    NXPLOG_NCIHAL_E("Invalid Timer Id, Timer Creation failed");
    return NFCSTATUS_FAILED;
  }

  if (phNxpNciHal_init_monitor() == NULL) {
    NXPLOG_NCIHAL_E("Init monitor failed");
    return NFCSTATUS_FAILED;
  }

  CONCURRENCY_LOCK();
  memset(&nxpncihal_ctrl, 0x00, sizeof(nxpncihal_ctrl));
  memset(&tOsalConfig, 0x00, sizeof(tOsalConfig));
  memset(&tTmlConfig, 0x00, sizeof(tTmlConfig));
  memset(&nxpprofile_ctrl, 0, sizeof(phNxpNciProfile_Control_t));

  /* By default HAL status is HAL_STATUS_OPEN */
  nxpncihal_ctrl.halStatus = HAL_STATUS_OPEN;
  nxpncihal_ctrl.is_wait_for_ce_ntf = false;
  nxpncihal_ctrl.p_nfc_stack_cback = p_cback;
  nxpncihal_ctrl.p_nfc_stack_data_cback = p_data_cback;
  nxpncihal_ctrl.hal_boot_mode = boot_mode;

  /*nci version NCI_VERSION_UNKNOWN version by default*/
  nxpncihal_ctrl.nci_info.nci_version = NCI_VERSION_UNKNOWN;

  /*Structure related to set config management*/
  mGetCfg_info = NULL;
  mGetCfg_info = nxp_malloc(sizeof(phNxpNci_getCfg_info_t));
  if (mGetCfg_info == NULL) {
    goto clean_and_return;
  }
  memset(mGetCfg_info, 0x00, sizeof(phNxpNci_getCfg_info_t));

  /*Read the nfc device node name*/
  nfc_dev_node = (uint8_t*)nxp_malloc(max_len * sizeof(uint8_t));
  if (nfc_dev_node == NULL) {
    NXPLOG_NCIHAL_E("malloc of nfc_dev_node failed ");
    goto clean_and_return;
  } else if (!GetNxpStrValue(NAME_NXP_NFC_DEV_NODE, (char*)nfc_dev_node,
                             max_len)) {
    NXPLOG_NCIHAL_E(
        "Invalid nfc device node name keeping the default device node "
        "/dev/pn54x");
    strcpy((char*)nfc_dev_node, "/dev/pn54x");
  }
  /* Configure hardware link */
  nxpncihal_ctrl.gDrvCfg.nClientId = phDal4Nfc_msgget(0, 0600);
  nxpncihal_ctrl.gDrvCfg.nLinkType = ENUM_LINK_TYPE_I2C; /* For PN54X */
  tTmlConfig.pDevName = (int8_t*)nfc_dev_node;
  tOsalConfig.dwCallbackThreadId = (uintptr_t)nxpncihal_ctrl.gDrvCfg.nClientId;
  tOsalConfig.pLogFile = NULL;
  tTmlConfig.dwGetMsgThreadId = (uintptr_t)nxpncihal_ctrl.gDrvCfg.nClientId;

  /* Initialize TML layer */
  wConfigStatus = phTmlNfc_Init(&tTmlConfig);
  if (wConfigStatus != NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_E("phTmlNfc_Init Failed");
    goto clean_and_return;
  } else {
    if (nfc_dev_node != NULL) {
      free(nfc_dev_node);
      nfc_dev_node = NULL;
    }
  }
  /* Create the client thread */
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  ret_val = pthread_create(&nxpncihal_ctrl.client_thread, &attr,
                           phNxpNciHal_client_thread, &nxpncihal_ctrl);
  pthread_attr_destroy(&attr);
  if (ret_val != 0) {
    NXPLOG_NCIHAL_E("pthread_create failed");
    wConfigStatus = phTmlNfc_Shutdown();
    goto clean_and_return;
  }

  CONCURRENCY_UNLOCK();
  /* call read pending */
  status = phTmlNfc_Read(
      nxpncihal_ctrl.p_cmd_data, NCI_MAX_DATA_LEN,
      (pphTmlNfc_TransactCompletionCb_t)&phNxpNciHal_read_complete, NULL);
  if (status != NFCSTATUS_PENDING) {
    NXPLOG_NCIHAL_E("TML Read status error status = %x", status);
    wConfigStatus = phTmlNfc_Shutdown();
    wConfigStatus = NFCSTATUS_FAILED;
    goto clean_and_return;
  }
  init_retry:

    phNxpNciHal_ext_init();

    status = phNxpNciHal_send_ext_cmd(sizeof(cmd_reset_nci), cmd_reset_nci);
    if ((status != NFCSTATUS_SUCCESS) &&
        (nxpncihal_ctrl.retry_cnt >= MAX_RETRY_COUNT)) {
      NXPLOG_NCIHAL_E("NFCC not coming out from Standby");
      wConfigStatus = NFCSTATUS_FAILED;
    } else if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NCI_CORE_RESET: Failed");
      if (init_retry_cnt < 3) {
        init_retry_cnt++;
        (void)phNxpNciHal_power_cycle();
        goto init_retry;
      } else
        init_retry_cnt = 0;
      wConfigStatus = phTmlNfc_Shutdown();
      wConfigStatus = NFCSTATUS_FAILED;
      goto clean_and_return;
    }
    if(nxpncihal_ctrl.nci_info.nci_version == NCI_VERSION_2_0) {
        status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci2_0), cmd_init_nci2_0);
    } else {
        status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci), cmd_init_nci);
        if(status == NFCSTATUS_SUCCESS && (nfcFL.chipType == pn557)) {
            NXPLOG_NCIHAL_E("Chip is in NCI1.0 mode reset the chip to 2.0 mode");
            status = phNxpNciHal_send_ext_cmd(sizeof(cmd_reset_nci), cmd_reset_nci);
            if(status == NFCSTATUS_SUCCESS) {
                status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci2_0), cmd_init_nci2_0);
                if(status == NFCSTATUS_SUCCESS) {
                    goto init_retry;
                }
            }
        }
    }

    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("COMMAND Failed");
      if (init_retry_cnt < 3) {
        init_retry_cnt++;
        (void)phNxpNciHal_power_cycle();
        goto init_retry;
      } else
        init_retry_cnt = 0;
      wConfigStatus = phTmlNfc_Shutdown();
      wConfigStatus = NFCSTATUS_FAILED;
      goto clean_and_return;
    }
  /* Call open complete */
  phNxpNciHal_open_complete(wConfigStatus);

  return wConfigStatus;

clean_and_return:
  CONCURRENCY_UNLOCK();
  if (nfc_dev_node != NULL) {
    free(nfc_dev_node);
    nfc_dev_node = NULL;
  }
  if (mGetCfg_info != NULL) {
    free(mGetCfg_info);
    mGetCfg_info = NULL;
  }
  /* Report error status */
  (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_OPEN_CPLT_EVT,
                                      HAL_NFC_STATUS_FAILED);
  nxpncihal_ctrl.p_nfc_stack_cback = NULL;
  nxpncihal_ctrl.p_nfc_stack_data_cback = NULL;
  phNxpNciHal_cleanup_monitor();
  nxpncihal_ctrl.halStatus = HAL_STATUS_CLOSE;
  return NFCSTATUS_FAILED;
}

/******************************************************************************
 * Function         phNxpNciHal_fw_mw_check
 *
 * Description      This function inform the status of phNxpNciHal_fw_mw_check
 *                  function to libnfc-nci.
 *
 * Returns          int.
 *
 ******************************************************************************/
int phNxpNciHal_fw_mw_ver_check() {
    NFCSTATUS status = NFCSTATUS_FAILED;

    if (((nfcFL.chipType == pn553)||(nfcFL.chipType == pn80T)) &&
            (rom_version == 0x11) && (fw_maj_ver == 0x01)) {
        status = NFCSTATUS_SUCCESS;
    } else if (((nfcFL.chipType == pn551)||(nfcFL.chipType == pn67T)) &&
            (rom_version == 0x10) && (fw_maj_ver == 0x05)) {
        status = NFCSTATUS_SUCCESS;
    } else if (((nfcFL.chipType == pn548C2)||(nfcFL.chipType == pn66T)) &&
            (rom_version == 0x10) && (fw_maj_ver == 0x01)) {
        status = NFCSTATUS_SUCCESS;
    } else if (((nfcFL.chipType == pn547C2)||(nfcFL.chipType == pn65T)) &&
            (rom_version == 0x08) && (fw_maj_ver == 0x01)) {
        status = NFCSTATUS_SUCCESS;
    }
    return status;
}
/******************************************************************************
 * Function         phNxpNciHal_open_complete
 *
 * Description      This function inform the status of phNxpNciHal_open
 *                  function to libnfc-nci.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_open_complete(NFCSTATUS status) {
  static phLibNfc_Message_t msg;

  if (status == NFCSTATUS_SUCCESS) {
    msg.eMsgType = NCI_HAL_OPEN_CPLT_MSG;
    nxpncihal_ctrl.hal_open_status = true;
  } else {
    msg.eMsgType = NCI_HAL_ERROR_MSG;
  }

  msg.pMsgData = NULL;
  msg.Size = 0;

  phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId,
                        (phLibNfc_Message_t*)&msg);

  return;
}

/******************************************************************************
 * Function         phNxpNciHal_write
 *
 * Description      This function write the data to NFCC through physical
 *                  interface (e.g. I2C) using the PN54X driver interface.
 *                  Before sending the data to NFCC, phNxpNciHal_write_ext
 *                  is called to check if there is any extension processing
 *                  is required for the NCI packet being sent out.
 *
 * Returns          It returns number of bytes successfully written to NFCC.
 *
 ******************************************************************************/
int phNxpNciHal_write(uint16_t data_len, const uint8_t* p_data) {
  NFCSTATUS status = NFCSTATUS_FAILED;
  static phLibNfc_Message_t msg;

  CONCURRENCY_LOCK();

  if (nxpncihal_ctrl.halStatus != HAL_STATUS_OPEN) {
    return NFCSTATUS_FAILED;
  }

  /* Create local copy of cmd_data */
  memcpy(nxpncihal_ctrl.p_cmd_data, p_data, data_len);
  nxpncihal_ctrl.cmd_len = data_len;
  if (nxpncihal_ctrl.cmd_len > NCI_MAX_DATA_LEN) {
    NXPLOG_NCIHAL_D("cmd_len exceeds limit NCI_MAX_DATA_LEN");
    goto clean_and_return;
  }
#ifdef P2P_PRIO_LOGIC_HAL_IMP
  /* Specific logic to block RF disable when P2P priority logic is busy */
  if (p_data[0] == 0x21 && p_data[1] == 0x06 && p_data[2] == 0x01 &&
      EnableP2P_PrioLogic == true) {
    NXPLOG_NCIHAL_D("P2P priority logic busy: Disable it.");
    phNxpNciHal_clean_P2P_Prio();
  }
#endif

  /* Check for NXP ext before sending write */
  status =
      phNxpNciHal_write_ext(&nxpncihal_ctrl.cmd_len, nxpncihal_ctrl.p_cmd_data,
                            &nxpncihal_ctrl.rsp_len, nxpncihal_ctrl.p_rsp_data);
  if (status != NFCSTATUS_SUCCESS) {
    /* Do not send packet to PN54X, send response directly */
    msg.eMsgType = NCI_HAL_RX_MSG;
    msg.pMsgData = NULL;
    msg.Size = 0;

    phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId,
                          (phLibNfc_Message_t*)&msg);
    goto clean_and_return;
  }

  data_len = phNxpNciHal_write_unlocked(nxpncihal_ctrl.cmd_len,
                                        nxpncihal_ctrl.p_cmd_data);

  if (icode_send_eof == 1) {
    usleep(10000);
    icode_send_eof = 2;
    phNxpNciHal_send_ext_cmd(3, cmd_icode_eof);
  }

clean_and_return:
  CONCURRENCY_UNLOCK();
  /* No data written */
  return data_len;
}

/******************************************************************************
 * Function         phNxpNciHal_write_unlocked
 *
 * Description      This is the actual function which is being called by
 *                  phNxpNciHal_write. This function writes the data to NFCC.
 *                  It waits till write callback provide the result of write
 *                  process.
 *
 * Returns          It returns number of bytes successfully written to NFCC.
 *
 ******************************************************************************/
int phNxpNciHal_write_unlocked(uint16_t data_len, const uint8_t* p_data) {
  NFCSTATUS status = NFCSTATUS_INVALID_PARAMETER;
  phNxpNciHal_Sem_t cb_data;
  nxpncihal_ctrl.retry_cnt = 0;
  static uint8_t reset_ntf[] = {0x60, 0x00, 0x06, 0xA0, 0x00,
                                0xC7, 0xD4, 0x00, 0x00};

  /* Create the local semaphore */
  if (phNxpNciHal_init_cb_data(&cb_data, NULL) != NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_D("phNxpNciHal_write_unlocked Create cb data failed");
    data_len = 0;
    goto clean_and_return;
  }

  /* Create local copy of cmd_data */
  memcpy(nxpncihal_ctrl.p_cmd_data, p_data, data_len);
  nxpncihal_ctrl.cmd_len = data_len;

retry:

  data_len = nxpncihal_ctrl.cmd_len;

  status = phTmlNfc_Write(
      (uint8_t*)nxpncihal_ctrl.p_cmd_data, (uint16_t)nxpncihal_ctrl.cmd_len,
      (pphTmlNfc_TransactCompletionCb_t)&phNxpNciHal_write_complete,
      (void*)&cb_data);
  if (status != NFCSTATUS_PENDING) {
    NXPLOG_NCIHAL_E("write_unlocked status error");
    data_len = 0;
    goto clean_and_return;
  }

  /* Wait for callback response */
  if (SEM_WAIT(cb_data)) {
    NXPLOG_NCIHAL_E("write_unlocked semaphore error");
    data_len = 0;
    goto clean_and_return;
  }

  if (cb_data.status != NFCSTATUS_SUCCESS) {
    data_len = 0;
    if (nxpncihal_ctrl.retry_cnt++ < MAX_RETRY_COUNT) {
      NXPLOG_NCIHAL_E(
          "write_unlocked failed - PN54X Maybe in Standby Mode - Retry");
      if(nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT) {
          /* 5ms delay to give NFCC wake up delay */
          usleep(5000);
      } else {
          /* 1ms delay to give NFCC wake up delay */
          usleep(1000);
}
      goto retry;
    } else {
      NXPLOG_NCIHAL_E(
          "write_unlocked failed - PN54X Maybe in Standby Mode (max count = "
          "0x%x)",
          nxpncihal_ctrl.retry_cnt);

      status = phTmlNfc_IoCtl(phTmlNfc_e_ResetDevice);

      if (NFCSTATUS_SUCCESS == status) {
        NXPLOG_NCIHAL_D("PN54X Reset - SUCCESS\n");
      } else {
        NXPLOG_NCIHAL_D("PN54X Reset - FAILED\n");
      }
      if (nxpncihal_ctrl.p_nfc_stack_data_cback != NULL &&
          nxpncihal_ctrl.hal_open_status == true) {
        if (nxpncihal_ctrl.p_rx_data != NULL) {
          NXPLOG_NCIHAL_D(
              "Send the Core Reset NTF to upper layer, which will trigger the "
              "recovery\n");
          // Send the Core Reset NTF to upper layer, which will trigger the
          // recovery.
          nxpncihal_ctrl.rx_data_len = sizeof(reset_ntf);
          memcpy(nxpncihal_ctrl.p_rx_data, reset_ntf, sizeof(reset_ntf));
          (*nxpncihal_ctrl.p_nfc_stack_data_cback)(nxpncihal_ctrl.rx_data_len,
                                                   nxpncihal_ctrl.p_rx_data);
        } else {
          (*nxpncihal_ctrl.p_nfc_stack_data_cback)(0x00, NULL);
        }
        write_unlocked_status = NFCSTATUS_FAILED;
      }
    }
  } else {
    write_unlocked_status = NFCSTATUS_SUCCESS;
  }

clean_and_return:
  phNxpNciHal_cleanup_cb_data(&cb_data);
  return data_len;
}

/******************************************************************************
 * Function         phNxpNciHal_write_complete
 *
 * Description      This function handles write callback.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_write_complete(void* pContext,
                                       phTmlNfc_TransactInfo_t* pInfo) {
  phNxpNciHal_Sem_t* p_cb_data = (phNxpNciHal_Sem_t*)pContext;

  if (pInfo->wStatus == NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_D("write successful status = 0x%x", pInfo->wStatus);
  } else {
    NXPLOG_NCIHAL_E("write error status = 0x%x", pInfo->wStatus);
  }

  p_cb_data->status = pInfo->wStatus;

  SEM_POST(p_cb_data);

  return;
}

/******************************************************************************
 * Function         phNxpNciHal_read_complete
 *
 * Description      This function is called whenever there is an NCI packet
 *                  received from NFCC. It could be RSP or NTF packet. This
 *                  function provide the received NCI packet to libnfc-nci
 *                  using data callback of libnfc-nci.
 *                  There is a pending read called from each
 *                  phNxpNciHal_read_complete so each a packet received from
 *                  NFCC can be provide to libnfc-nci.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_read_complete(void* pContext,
                                      phTmlNfc_TransactInfo_t* pInfo) {
  NFCSTATUS status = NFCSTATUS_FAILED;
  UNUSED(pContext);
  if (nxpncihal_ctrl.read_retry_cnt == 1) {
    nxpncihal_ctrl.read_retry_cnt = 0;
  }
  if (nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT &&
          (pInfo->wStatus == NFCSTATUS_READ_FAILED)) {
      if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
          read_failed_disable_nfc = true;
          /* Send the event */
          (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_ERROR_EVT,
                  HAL_NFC_STATUS_ERR_CMD_TIMEOUT);
      }
      return;
  }

  if (pInfo->wStatus == NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_D("read successful status = 0x%x", pInfo->wStatus);

    nxpncihal_ctrl.p_rx_data = pInfo->pBuff;
    nxpncihal_ctrl.rx_data_len = pInfo->wLength;

    status = phNxpNciHal_process_ext_rsp(nxpncihal_ctrl.p_rx_data,
                                         &nxpncihal_ctrl.rx_data_len);

    phNxpNciHal_print_res_status(nxpncihal_ctrl.p_rx_data,
                                 &nxpncihal_ctrl.rx_data_len);
#if (NXP_NFCC_FORCE_NCI1_0_INIT == true)
    /* Notification Checking */
    if (nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT && ((nxpncihal_ctrl.hal_ext_enabled == 1) &&
        (nxpncihal_ctrl.p_rx_data[0x00] == 0x60) &&
        (nxpncihal_ctrl.p_rx_data[0x03] == 0x02))) {
      nxpncihal_ctrl.ext_cb_data.status = NFCSTATUS_SUCCESS;
      SEM_POST(&(nxpncihal_ctrl.ext_cb_data));
    } else
#endif
        /* Check if response should go to hal module only */
        if (nxpncihal_ctrl.hal_ext_enabled == 1 &&
            ((nxpncihal_ctrl.p_rx_data[0x00] & 0xF0) == 0x40 ||
             ((icode_detected == true) && (icode_send_eof == 3)))) {
      if (status == NFCSTATUS_FAILED) {
        NXPLOG_NCIHAL_D("enter into NFCC init recovery");
        nxpncihal_ctrl.ext_cb_data.status = status;
      }
      /* Unlock semaphore */
      SEM_POST(&(nxpncihal_ctrl.ext_cb_data));
    }
     //Notification Checking
    else if((nxpncihal_ctrl.hal_ext_enabled == TRUE)    &&
            ((nxpncihal_ctrl.p_rx_data[0x00]  & NCI_MT_MASK) == NCI_MT_NTF) &&
            (nxpncihal_ctrl.nci_info.wait_for_ntf == TRUE)) {
        /* Unlock semaphore waiting for only  ntf*/
      SEM_POST(&(nxpncihal_ctrl.ext_cb_data));
      nxpncihal_ctrl.nci_info.wait_for_ntf = FALSE;
    }
    /* Read successful send the event to higher layer */
    else if ((nxpncihal_ctrl.p_nfc_stack_data_cback != NULL) &&
             (status == NFCSTATUS_SUCCESS) && (send_to_upper_kovio == 1)) {
      (*nxpncihal_ctrl.p_nfc_stack_data_cback)(nxpncihal_ctrl.rx_data_len,
                                               nxpncihal_ctrl.p_rx_data);
    }
  } else {
    NXPLOG_NCIHAL_E("read error status = 0x%x", pInfo->wStatus);
  }

  if (nxpncihal_ctrl.halStatus == HAL_STATUS_CLOSE &&
    nxpncihal_ctrl.nci_info.wait_for_ntf == FALSE) {
    NXPLOG_NCIHAL_E(" Ignoring read , HAL close triggered");
    return;
  }
  /* Read again because read must be pending always.*/
  status = phTmlNfc_Read(
      Rx_data, NCI_MAX_DATA_LEN,
      (pphTmlNfc_TransactCompletionCb_t)&phNxpNciHal_read_complete, NULL);
  if (status != NFCSTATUS_PENDING) {
    NXPLOG_NCIHAL_E("read status error status = %x", status);
    /* TODO: Not sure how to handle this ? */
  }

  return;
}

void read_retry() {
  /* Read again because read must be pending always.*/
  NFCSTATUS status = phTmlNfc_Read(
      Rx_data, NCI_MAX_DATA_LEN,
      (pphTmlNfc_TransactCompletionCb_t)&phNxpNciHal_read_complete, NULL);
  if (status != NFCSTATUS_PENDING) {
    NXPLOG_NCIHAL_E("read status error status = %x", status);
    /* TODO: Not sure how to handle this ? */
  }
}
/*******************************************************************************
**
** Function         phNxpNciHal_check_delete_nfaStorage_DHArea
**
** Description      check the file and delete if present.
**
**
** Returns          void
**
*******************************************************************************/
void phNxpNciHal_check_delete_nfaStorage_DHArea() {
  struct stat st;
  const char config_eseinfo_path[] = "/data/vendor/nfc/nfaStorage.bin1";
  if (stat(config_eseinfo_path, &st) == -1) {
    ALOGD("%s file not present = %s", __func__, config_eseinfo_path);
  } else {
    ALOGD("%s file present = %s", __func__, config_eseinfo_path);
    remove(config_eseinfo_path);
    ALOGD("%s Deleting the file present = %s", __func__, config_eseinfo_path);
  }
}

/******************************************************************************
 * Function         phNxpNciHal_core_initialized
 *
 * Description      This function is called by libnfc-nci after successful open
 *                  of NFCC. All proprietary setting for PN54X are done here.
 *                  After completion of proprietary settings notification is
 *                  provided to libnfc-nci through callback function.
 *
 * Returns          Always returns NFCSTATUS_SUCCESS (0).
 *
 ******************************************************************************/
int phNxpNciHal_core_initialized(uint8_t* p_core_init_rsp_params) {
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  uint8_t* buffer = NULL;
  uint8_t isfound = false;
#ifdef FW_DWNLD_FLAG
  uint8_t fw_dwnld_flag = false;
#endif
  uint8_t setConfigAlways = false;
  uint8_t temp_fix = 1;
  static uint8_t retry_core_init_cnt = 0;
  static uint8_t p2p_listen_mode_routing_cmd[] = {0x21, 0x01, 0x07, 0x00, 0x01,
                                                  0x01, 0x03, 0x00, 0x01, 0x05};
  static uint8_t swp_full_pwr_mode_on_cmd[] = {0x20, 0x02, 0x05, 0x01,
                                               0xA0, 0xF1, 0x01, 0x01};
  static uint8_t swp_switch_timeout_cmd[] = {0x20, 0x02, 0x06, 0x01, 0xA0,
                                             0xF3, 0x02, 0x00, 0x00};
  static uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  static uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  static uint8_t cmd_init_nci2_0[] = {0x20,0x01,0x02,0x00,0x00};
  config_success = true;
  long bufflen = 260;
  long retlen = 0;
  unsigned long num = 0;
  phNxpNci_EEPROM_info_t mEEPROM_info = {.request_mode = 0};
  static uint8_t android_l_aid_matching_mode_on_cmd[] = {
      0x20, 0x02, 0x05, 0x01, 0xA0, 0x91, 0x01, 0x01};
  /*initialize dummy FW recovery variables*/
  if(nfcFL.chipType != pn547C2) {
      gRecFwRetryCount = 0;
      gRecFWDwnld = false;
  }
  if (nxpncihal_ctrl.halStatus != HAL_STATUS_OPEN) {
    return NFCSTATUS_FAILED;
  }

  NXPLOG_NCIHAL_D("phNxpNciHal_core_initialized::p_core_init_rsp_params : %d", *p_core_init_rsp_params);

  /*MW recovery -- begins*/
  if ((*p_core_init_rsp_params > 0) && (*p_core_init_rsp_params < 4)) {
  retry_core_init:
    config_access = false;
    if (mGetCfg_info != NULL) {
      mGetCfg_info->isGetcfg = false;
    }
    if (buffer != NULL) {
      free(buffer);
      buffer = NULL;
    }
    if (retry_core_init_cnt > 3) {
        if (nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT &&
                (nxpncihal_ctrl.p_nfc_stack_cback != NULL)) {
            NXPLOG_NCIHAL_D("Posting Core Init Failed\n");
            read_failed_disable_nfc = true;
            (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_ERROR_EVT,
                    HAL_NFC_STATUS_ERR_CMD_TIMEOUT);
        }
        return NFCSTATUS_FAILED;
    }

    status = phTmlNfc_IoCtl(phTmlNfc_e_ResetDevice);
    if (NFCSTATUS_SUCCESS == status) {
      NXPLOG_NCIHAL_D("PN54X Reset - SUCCESS\n");
    } else {
      NXPLOG_NCIHAL_D("PN54X Reset - FAILED\n");
    }

    if(nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT) {
        status = phNxpNciHal_send_ext_cmd_ntf(sizeof(cmd_reset_nci), cmd_reset_nci);
    }
    else {
        status = phNxpNciHal_send_ext_cmd(sizeof(cmd_reset_nci), cmd_reset_nci);
    }

    if ((status != NFCSTATUS_SUCCESS) &&
        (nxpncihal_ctrl.retry_cnt >= MAX_RETRY_COUNT)) {
      NXPLOG_NCIHAL_E(
          "NFCC not coming out from Standby Max retry count reached");
      retry_core_init_cnt++;
      goto retry_core_init;
    } else if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NCI_CORE_RESET: Failed");
      retry_core_init_cnt++;
      goto retry_core_init;
    }

    if (*p_core_init_rsp_params == 2) {
      NXPLOG_NCIHAL_E(" Last command is CORE_RESET!!");
      goto invoke_callback;
    }

    if (nxpncihal_ctrl.nci_info.nci_version == NCI_VERSION_2_0) {
      status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci2_0), cmd_init_nci2_0);
    } else {
      status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci), cmd_init_nci);
    }
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NCI_CORE_INIT : Failed");
      retry_core_init_cnt++;
      goto retry_core_init;
    }

    if (*p_core_init_rsp_params == 3) {
      NXPLOG_NCIHAL_E(" Last command is CORE_INIT!!");
      goto invoke_callback;
    }
  }

  /*MW recovery --ended*/

  buffer = (uint8_t*)nxp_malloc(bufflen * sizeof(uint8_t));
  if (NULL == buffer) {
    return NFCSTATUS_FAILED;
  }

  config_access = true;
  retlen = 0;
  isfound = GetNxpByteArrayValue(NAME_NXP_ACT_PROP_EXTN, (char*)buffer, bufflen,
                                 &retlen);
  if (retlen > 0) {
    /* NXP ACT Proprietary Ext */
    status = phNxpNciHal_send_ext_cmd(retlen, buffer);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NXP ACT Proprietary Ext failed");
      retry_core_init_cnt++;
      goto retry_core_init;
    }
  }

  retlen = 0;
  isfound = GetNxpByteArrayValue(NAME_NXP_CORE_STANDBY, (char*)buffer, bufflen,
                                 &retlen);
  if (retlen > 0) {
    /* NXP ACT Proprietary Ext */
    status = phNxpNciHal_send_ext_cmd(retlen, buffer);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("Stand by mode enable failed");
      retry_core_init_cnt++;
      goto retry_core_init;
    }
  }

  if(nfcFL.eseFL._ESE_SVDD_SYNC) {
      if (GetNxpNumValue(NAME_NXP_SVDD_SYNC_OFF_DELAY, (void*)&gSvddSyncOff_Delay,
              sizeof(gSvddSyncOff_Delay))) {
          if (gSvddSyncOff_Delay > 20) gSvddSyncOff_Delay = 10;
          NXPLOG_NCIHAL_E("NAME_NXP_SVDD_SYNC_OFF_DELAY success value = %d",
                  gSvddSyncOff_Delay);
      } else {
          NXPLOG_NCIHAL_E("NAME_NXP_SVDD_SYNC_OFF_DELAY failed");
          gSvddSyncOff_Delay = 10;
      }
  }
  config_access = false;
  phNxpNciHal_check_factory_reset();

#ifdef FW_DWNLD_FLAG
  mEEPROM_info.buffer = &fw_dwnld_flag;
  mEEPROM_info.bufflen = sizeof(fw_dwnld_flag);
  mEEPROM_info.request_type = EEPROM_FW_DWNLD;
  mEEPROM_info.request_mode = GET_EEPROM_DATA;
  request_EEPROM(&mEEPROM_info);
#endif


  if((nfcFL.chipType != pn547C2) && nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT_PROP) {
      if (isNxpConfigModified() || (fw_dwnld_flag == 0x01)) {
          uint8_t value;
          retlen = 0;
          if (GetNxpNumValue(NAME_NXP_PROP_BLACKLIST_ROUTING,
                  (void*)&retlen, sizeof(retlen))) {
              if (retlen == 0x00 || retlen == 0x01) {
                  value = (uint8_t)retlen;
                  mEEPROM_info.buffer = &value;
                  mEEPROM_info.bufflen = sizeof(value);
                  mEEPROM_info.request_type = EEPROM_PROP_ROUTING;
                  mEEPROM_info.request_mode = SET_EEPROM_DATA;
                  status = request_EEPROM(&mEEPROM_info);
              }
          }
      }
}

  if((nfcFL.chipType != pn547C2) && (nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME ==
          nfcFL.eseFL._ESE_WIRED_MODE_RESUME)) {
      uint8_t resume_timeout_buf[NXP_WIREDMODE_RESUME_TIMEOUT_LEN];
      mEEPROM_info.request_mode = GET_EEPROM_DATA;
      NXPLOG_NCIHAL_D("Timeout value");
      if (isNxpConfigModified() || (fw_dwnld_flag == 0x01)) {
          NXPLOG_NCIHAL_D("Timeout value - 1");
          if (GetNxpByteArrayValue(NAME_NXP_WIREDMODE_RESUME_TIMEOUT, (char*)buffer,
                  bufflen, &retlen)) {
              NXPLOG_NCIHAL_D("Time out value %x %x %x %x retlen=%ld", buffer[0],
                      buffer[1], buffer[2], buffer[3], retlen);
              if (retlen >= NXP_WIREDMODE_RESUME_TIMEOUT_LEN) {
                  memcpy(&resume_timeout_buf, buffer, NXP_STAG_TIMEOUT_BUF_LEN);
                  mEEPROM_info.request_mode = SET_EEPROM_DATA;
              }
          }
      }
      mEEPROM_info.buffer = resume_timeout_buf;
      mEEPROM_info.bufflen = sizeof(resume_timeout_buf);
      mEEPROM_info.request_type = EEPROM_WIREDMODE_RESUME_TIMEOUT;
      request_EEPROM(&mEEPROM_info);
  }

#if (NXP_EXTNS == TRUE)
  if(nfcFL.eseFL._ESE_POWER_MODE)
  {
    uint8_t value;
    retlen = 0;
    if (GetNxpNumValue(NAME_NXP_ESE_POWER_DH_CONTROL, (void*)&retlen,
                       sizeof(retlen))) {
      if (retlen == 0x01 || retlen == 0x02) {
        value = (uint8_t)retlen;
        if (value == 2) value = 0;
        mEEPROM_info.buffer = &value;
        mEEPROM_info.bufflen = sizeof(value);
        mEEPROM_info.request_type = EEPROM_ESE_SVDD_POWER;
        mEEPROM_info.request_mode = SET_EEPROM_DATA;

        if (isNxpConfigModified() || (fw_dwnld_flag == 0x01))
          status = request_EEPROM(&mEEPROM_info);
      }
      if (retlen == 0x01) {
        retlen = 0;
        value = 0x40;
        mEEPROM_info.buffer = &value;
        mEEPROM_info.bufflen = sizeof(value);
        mEEPROM_info.request_type = EEPROM_ESE_POWER_EXT_PMU;
        mEEPROM_info.request_mode = SET_EEPROM_DATA;
        phTmlNfc_IoCtl(phTmlNfc_e_SetLegacyPowerScheme);

        if (isNxpConfigModified() || (fw_dwnld_flag == 0x01))
          status = request_EEPROM(&mEEPROM_info);
      } else if (retlen == 0x02) {
        retlen = 0;
        value = 0;
        if (GetNxpNumValue(NAME_NXP_ESE_POWER_EXT_PMU, (void*)&retlen,
                           sizeof(retlen))) {
          if (retlen == 0x01 || retlen == 0x02) {
            value = (uint8_t)retlen;
            if (value == 1) {
              value = 0x50;
            } else {
              value = 0x48;
            }
            phTmlNfc_IoCtl(phTmlNfc_e_SetExtPMUPowerScheme);
            mEEPROM_info.buffer = &value;
            mEEPROM_info.bufflen = sizeof(value);
            mEEPROM_info.request_type = EEPROM_ESE_POWER_EXT_PMU;
            mEEPROM_info.request_mode = SET_EEPROM_DATA;
            if (isNxpConfigModified() || (fw_dwnld_flag == 0x01))
              status = request_EEPROM(&mEEPROM_info);
          }
        }
      }
    }
  }
#endif
  setConfigAlways = false;
  isfound = GetNxpNumValue(NAME_NXP_SET_CONFIG_ALWAYS, &num, sizeof(num));
  if (isfound > 0) {
    setConfigAlways = num;
  }
  NXPLOG_NCIHAL_D("EEPROM_fw_dwnld_flag : 0x%02x SetConfigAlways flag : 0x%02x",
                  fw_dwnld_flag, setConfigAlways);

  if (fw_dwnld_flag == 0x01) {
    phNxpNciHal_check_delete_nfaStorage_DHArea();
  }

  if ((true == fw_dwnld_flag) || (true == setConfigAlways) ||
      isNxpRFConfigModified() || isNxpConfigModified()) {
    config_access = true;
    retlen = 0;
    status = phNxpNciHal_check_clock_config();
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("phNxpNciHal_check_clock_config failed");
      retry_core_init_cnt++;
      goto retry_core_init;
    }

  if (config_success == false) return NFCSTATUS_FAILED;
#ifdef PN547C2_CLOCK_SETTING
    if (isNxpConfigModified() || (phNxpNciClock.issetConfig)
#if (NFC_NXP_HFO_SETTINGS == TRUE)
        || temp_fix == 1
#endif
        ) {
      // phNxpNciHal_get_clk_freq();
      phNxpNciHal_set_clock();
      phNxpNciClock.issetConfig = false;
#if (NFC_NXP_HFO_SETTINGS == TRUE)
      if (temp_fix == 1) {
        NXPLOG_NCIHAL_D(
            "Applying Default Clock setting and DPLL register at power on");
        /*
        # A0, 0D, 06, 06, 83, 55, 2A, 04, 00 RF_CLIF_CFG_TARGET
        CLIF_DPLL_GEAR_REG
        # A0, 0D, 06, 06, 82, 33, 14, 17, 00 RF_CLIF_CFG_TARGET
        CLIF_DPLL_INIT_REG
        # A0, 0D, 06, 06, 84, AA, 85, 00, 80 RF_CLIF_CFG_TARGET
        CLIF_DPLL_INIT_FREQ_REG
        # A0, 0D, 06, 06, 81, 63, 00, 00, 00 RF_CLIF_CFG_TARGET
        CLIF_DPLL_CONTROL_REG
        */
        static uint8_t cmd_dpll_set_reg_nci[] = {
            0x20, 0x02, 0x25, 0x04, 0xA0, 0x0D, 0x06, 0x06, 0x83, 0x55,
            0x2A, 0x04, 0x00, 0xA0, 0x0D, 0x06, 0x06, 0x82, 0x33, 0x14,
            0x17, 0x00, 0xA0, 0x0D, 0x06, 0x06, 0x84, 0xAA, 0x85, 0x00,
            0x80, 0xA0, 0x0D, 0x06, 0x06, 0x81, 0x63, 0x00, 0x00, 0x00};

        status = phNxpNciHal_send_ext_cmd(sizeof(cmd_dpll_set_reg_nci),
                                          cmd_dpll_set_reg_nci);
        if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("NXP DPLL REG ACT Proprietary Ext failed");
          retry_core_init_cnt++;
          goto retry_core_init;
        }
        /* reset the NFCC after applying the clock setting and DPLL setting */
        // phTmlNfc_IoCtl(phTmlNfc_e_ResetDevice);
        temp_fix = 0;
        goto retry_core_init;
      }
#endif
    }
#endif

    retlen = 0;
    config_access = true;
    isfound = GetNxpByteArrayValue(NAME_NXP_NFC_PROFILE_EXTN, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      /* NXP ACT Proprietary Ext */
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("NXP ACT Proprietary Ext failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }
    }

    retlen = 0;
    if(nfcFL.chipType != pn547C2) {
        NXPLOG_NCIHAL_D("Performing TVDD Settings");
        /*TVDD settings commented for PN553 bringup FW  */
        isfound = GetNxpNumValue(NAME_NXP_EXT_TVDD_CFG, &num, sizeof(num));
        if (isfound > 0) {
            if (num == 1) {
                isfound = GetNxpByteArrayValue(NAME_NXP_EXT_TVDD_CFG_1, (char*)buffer,
                        bufflen, &retlen);
                if (retlen > 0) {
                    status = phNxpNciHal_send_ext_cmd(retlen, buffer);
                    if (status != NFCSTATUS_SUCCESS) {
                        NXPLOG_NCIHAL_E("EXT TVDD CFG 1 Settings failed");
                        retry_core_init_cnt++;
                        goto retry_core_init;
                    }
                }
            } else if (num == 2) {
                isfound = GetNxpByteArrayValue(NAME_NXP_EXT_TVDD_CFG_2, (char*)buffer,
                        bufflen, &retlen);
                if (retlen > 0) {
                    status = phNxpNciHal_send_ext_cmd(retlen, buffer);
                    if (status != NFCSTATUS_SUCCESS) {
                        NXPLOG_NCIHAL_E("EXT TVDD CFG 2 Settings failed");
                        retry_core_init_cnt++;
                        goto retry_core_init;
                    }
                }
            } else if (num == 3) {
                isfound = GetNxpByteArrayValue(NAME_NXP_EXT_TVDD_CFG_3, (char*)buffer,
                        bufflen, &retlen);
                if (retlen > 0) {
                    status = phNxpNciHal_send_ext_cmd(retlen, buffer);
                    if (status != NFCSTATUS_SUCCESS) {
                        NXPLOG_NCIHAL_E("EXT TVDD CFG 3 Settings failed");
                        retry_core_init_cnt++;
                        goto retry_core_init;
                    }
                }
            } else {
                NXPLOG_NCIHAL_E("Wrong Configuration Value %ld", num);
            }
        }
}

    retlen = 0;

    if(nfcFL.chipType != pn547C2) {
        config_access = false;
    }
    NXPLOG_NCIHAL_D("Performing RF Settings BLK 1");
    isfound = GetNxpByteArrayValue(NAME_NXP_RF_CONF_BLK_1, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if ((nfcFL.chipType != pn547C2) && (status == NFCSTATUS_SUCCESS)) {
          status = phNxpNciHal_CheckRFCmdRespStatus();
          /*STATUS INVALID PARAM 0x09*/
          if (status == 0x09) {
              phNxpNciHalRFConfigCmdRecSequence();
              retry_core_init_cnt++;
              goto retry_core_init;
          }
      } else if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("RF Settings BLK 1 failed");
          retry_core_init_cnt++;
          goto retry_core_init;
      }
    }
    retlen = 0;

    NXPLOG_NCIHAL_D("Performing RF Settings BLK 2");
    isfound = GetNxpByteArrayValue(NAME_NXP_RF_CONF_BLK_2, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if ((nfcFL.chipType != pn547C2) && (status == NFCSTATUS_SUCCESS)) {
          status = phNxpNciHal_CheckRFCmdRespStatus();
          /*STATUS INVALID PARAM 0x09*/
          if (status == 0x09) {
              phNxpNciHalRFConfigCmdRecSequence();
              retry_core_init_cnt++;
              goto retry_core_init;
          }
      } else if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("RF Settings BLK 2 failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }
    }
    retlen = 0;

    NXPLOG_NCIHAL_D("Performing RF Settings BLK 3");
    isfound = GetNxpByteArrayValue(NAME_NXP_RF_CONF_BLK_3, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if ((nfcFL.chipType != pn547C2) && (status == NFCSTATUS_SUCCESS)) {
          status = phNxpNciHal_CheckRFCmdRespStatus();
          /*STATUS INVALID PARAM 0x09*/
          if (status == 0x09) {
              phNxpNciHalRFConfigCmdRecSequence();
              retry_core_init_cnt++;
              goto retry_core_init;
          }
      } else if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("RF Settings BLK 3 failed");
          retry_core_init_cnt++;
          goto retry_core_init;
      }
    }
    retlen = 0;

    NXPLOG_NCIHAL_D("Performing RF Settings BLK 4");
    isfound = GetNxpByteArrayValue(NAME_NXP_RF_CONF_BLK_4, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if ((nfcFL.chipType != pn547C2) && (status == NFCSTATUS_SUCCESS)) {
          status = phNxpNciHal_CheckRFCmdRespStatus();
          /*STATUS INVALID PARAM 0x09*/
          if (status == 0x09) {
              phNxpNciHalRFConfigCmdRecSequence();
              retry_core_init_cnt++;
              goto retry_core_init;
          }
      } else if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("RF Settings BLK 4 failed");
          retry_core_init_cnt++;
          goto retry_core_init;
      }
    }
    retlen = 0;

    NXPLOG_NCIHAL_D("Performing RF Settings BLK 5");
    isfound = GetNxpByteArrayValue(NAME_NXP_RF_CONF_BLK_5, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if ((nfcFL.chipType != pn547C2) && (status == NFCSTATUS_SUCCESS)) {
          status = phNxpNciHal_CheckRFCmdRespStatus();
          /*STATUS INVALID PARAM 0x09*/
          if (status == 0x09) {
              phNxpNciHalRFConfigCmdRecSequence();
              retry_core_init_cnt++;
              goto retry_core_init;
          }
      } else if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("RF Settings BLK 5 failed");
          retry_core_init_cnt++;
          goto retry_core_init;
      }
    }
    retlen = 0;

    NXPLOG_NCIHAL_D("Performing RF Settings BLK 6");
    isfound = GetNxpByteArrayValue(NAME_NXP_RF_CONF_BLK_6, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if ((nfcFL.chipType != pn547C2) && (status == NFCSTATUS_SUCCESS)) {
          status = phNxpNciHal_CheckRFCmdRespStatus();
          /*STATUS INVALID PARAM 0x09*/
          if (status == 0x09) {
              phNxpNciHalRFConfigCmdRecSequence();
              retry_core_init_cnt++;
              goto retry_core_init;
          }
      } else if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("RF Settings BLK 6 failed");
          retry_core_init_cnt++;
          goto retry_core_init;
      }
    }
    retlen = 0;
    if (nfcFL.chipType != pn547C2) {
        config_access = true;
    }

    NXPLOG_NCIHAL_D("Performing NAME_NXP_CORE_CONF_EXTN Settings");
    isfound = GetNxpByteArrayValue(NAME_NXP_CORE_CONF_EXTN, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      /* NXP ACT Proprietary Ext */
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("NXP Core configuration failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }
    }

    if(nfcFL.eseFL._ESE_ETSI12_PROP_INIT) {
        uint8_t swp_info_buff[2];
        uint8_t swp_intf_status = 0x00;
        uint8_t swp1A_intf_status = 0x00;
        NFCSTATUS status = NFCSTATUS_FAILED;
        phNxpNci_EEPROM_info_t swp_intf_info;

        memset(swp_info_buff, 0, sizeof(swp_info_buff));
        /*Read SWP1 data*/
        memset(&swp_intf_info, 0, sizeof(swp_intf_info));
        swp_intf_info.request_mode = GET_EEPROM_DATA;
        swp_intf_info.request_type = EEPROM_SWP1_INTF;
        swp_intf_info.buffer = &swp_intf_status;
        swp_intf_info.bufflen = sizeof(uint8_t);
        status = request_EEPROM(&swp_intf_info);
        if (status == NFCSTATUS_OK)
            swp_info_buff[0] = swp_intf_status;
        else {
            NXPLOG_NCIHAL_E("request_EEPROM error occured %d", status);
            retry_core_init_cnt++;
            goto retry_core_init;
        }
        if(nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH) {
            /*Read SWP1A data*/
            memset(&swp_intf_info, 0, sizeof(swp_intf_info));
            swp_intf_info.request_mode = GET_EEPROM_DATA;
            swp_intf_info.request_type = EEPROM_SWP1A_INTF;
            swp_intf_info.buffer = &swp1A_intf_status;
            swp_intf_info.bufflen = sizeof(uint8_t);
            status = request_EEPROM(&swp_intf_info);
            if (status == NFCSTATUS_OK)
                swp_info_buff[1] = swp1A_intf_status;
            else {
                NXPLOG_NCIHAL_E("request_EEPROM error occured %d", status);
                retry_core_init_cnt++;
                goto retry_core_init;
            }
        }
        phNxpNci_EEPROM_info_t mEEPROM_info = {.request_mode = 0};
        mEEPROM_info.buffer = swp_info_buff;
        mEEPROM_info.bufflen= sizeof(swp_info_buff);
        mEEPROM_info.request_type = EEPROM_RF_CFG;
        mEEPROM_info.request_mode = SET_EEPROM_DATA;
        status = request_EEPROM(&mEEPROM_info);
        NXPLOG_NCIHAL_D("Setting value %d %d", swp_info_buff[1], swp_info_buff[0]);
    }  // END_OF_NFC_NXP_ESE_ETSI12_PROP_INIT

    NXPLOG_NCIHAL_D("Performing NAME_NXP_CORE_CONF Settings");
    retlen = 0;
    isfound = GetNxpByteArrayValue(NAME_NXP_CORE_CONF, (char*)buffer, bufflen,
                                   &retlen);
    if (retlen > 0) {
      /* NXP ACT Proprietary Ext */
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("Core Set Config failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }
    }

    retlen = 0;
    isfound = GetNxpByteArrayValue(NAME_NXP_CORE_MFCKEY_SETTING, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
      /* NXP ACT Proprietary Ext */
      status = phNxpNciHal_send_ext_cmd(retlen, buffer);
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("Setting mifare keys failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }
    }

    retlen = 0;
    if (nfcFL.chipType != pn547C2) {
        config_access = false;
    }
    isfound = GetNxpByteArrayValue(NAME_NXP_CORE_RF_FIELD, (char*)buffer,
                                   bufflen, &retlen);
    if (retlen > 0) {
        /* NXP ACT Proprietary Ext */
        status = phNxpNciHal_send_ext_cmd(retlen, buffer);
        if ((nfcFL.chipType != pn547C2) && (status == NFCSTATUS_SUCCESS)) {
            status = phNxpNciHal_CheckRFCmdRespStatus();
            /*STATUS INVALID PARAM 0x09*/
            if (status == 0x09) {
                phNxpNciHalRFConfigCmdRecSequence();
                retry_core_init_cnt++;
                goto retry_core_init;
            }
        } else if (status != NFCSTATUS_SUCCESS) {
            NXPLOG_NCIHAL_E("Setting NXP_CORE_RF_FIELD status failed");
            retry_core_init_cnt++;
            goto retry_core_init;
        }
    }
    if (nfcFL.chipType != pn547C2) {
        config_access = true;
    }
    num = 0;

    if (nfcFL.chipType != pn547C2) {
        /* NXP SWP switch timeout Setting*/
        if (GetNxpNumValue(NAME_NXP_SWP_SWITCH_TIMEOUT, (void*)&num, sizeof(num))) {
            // Check the permissible range [0 - 60]
            if (num <= 60) {
                if (0 < num) {
                    uint16_t timeout = num * 1000;
                    unsigned int timeoutHx = 0x0000;

                    uint8_t tmpbuffer[10];
                    snprintf((char*)tmpbuffer, 10, "%04x", timeout);
                    sscanf((const char*)tmpbuffer, "%x", (unsigned int*)&timeoutHx);

                    swp_switch_timeout_cmd[7] = (timeoutHx & 0xFF);
                    swp_switch_timeout_cmd[8] = ((timeoutHx & 0xFF00) >> 8);
                }

                status = phNxpNciHal_send_ext_cmd(sizeof(swp_switch_timeout_cmd),
                        swp_switch_timeout_cmd);
                if (status != NFCSTATUS_SUCCESS) {
                    NXPLOG_NCIHAL_E("SWP switch timeout Setting Failed");
                    retry_core_init_cnt++;
                    goto retry_core_init;
                }
            } else {
                NXPLOG_NCIHAL_E("SWP switch timeout Setting Failed - out of range!");
            }
        }
    }
    if (nfcFL.chipType != pn547C2) {
        status = phNxpNciHal_set_china_region_configs();
        if (status != NFCSTATUS_SUCCESS) {
            NXPLOG_NCIHAL_E("phNxpNciHal_set_china_region_configs failed");
            retry_core_init_cnt++;
            goto retry_core_init;
        }
    }

    if (nfcFL.chipType == pn547C2) {
        status = phNxpNciHal_uicc_baud_rate();
        if (status != NFCSTATUS_SUCCESS) {
            NXPLOG_NCIHAL_E("Setting NXP_CORE_RF_FIELD status failed");
            retry_core_init_cnt++;
            goto retry_core_init;
        }
    }

    config_access = false;
    // if recovery mode and length of last command is 0 then only reset the P2P
    // listen mode routing.
    if ((*p_core_init_rsp_params > 0) && (*p_core_init_rsp_params < 4) &&
            p_core_init_rsp_params[35] == 0) {
      /* P2P listen mode routing */
      status = phNxpNciHal_send_ext_cmd(sizeof(p2p_listen_mode_routing_cmd),
                                        p2p_listen_mode_routing_cmd);
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("P2P listen mode routing failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }
    }

    num = 0;

    /* SWP FULL PWR MODE SETTING ON */
    if (GetNxpNumValue(NAME_NXP_SWP_FULL_PWR_ON, (void*)&num, sizeof(num))) {
      if (1 == num) {
        status = phNxpNciHal_send_ext_cmd(sizeof(swp_full_pwr_mode_on_cmd),
                                          swp_full_pwr_mode_on_cmd);
        if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("SWP FULL PWR MODE SETTING ON CMD FAILED");
          retry_core_init_cnt++;
          goto retry_core_init;
        }
      } else {
        swp_full_pwr_mode_on_cmd[7] = 0x00;
        status = phNxpNciHal_send_ext_cmd(sizeof(swp_full_pwr_mode_on_cmd),
                                          swp_full_pwr_mode_on_cmd);
        if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("SWP FULL PWR MODE SETTING OFF CMD FAILED");
          retry_core_init_cnt++;
          goto retry_core_init;
        }
      }
    }
    if(nfcFL.nfccFL._NFCC_AID_MATCHING_PLATFORM_CONFIG == true) {
        num = 0;
        /* Android L AID Matching Platform Setting*/
        if (GetNxpNumValue(NAME_AID_MATCHING_PLATFORM, (void*)&num, sizeof(num))) {
            if (1 == num) {
                status =
                        phNxpNciHal_send_ext_cmd(sizeof(android_l_aid_matching_mode_on_cmd),
                                android_l_aid_matching_mode_on_cmd);
                if (status != NFCSTATUS_SUCCESS) {
                    NXPLOG_NCIHAL_E("Android L AID Matching Platform Setting Failed");
                    retry_core_init_cnt++;
                    goto retry_core_init;
                }
            } else if (2 == num) {
                android_l_aid_matching_mode_on_cmd[7] = 0x00;
                status =
                        phNxpNciHal_send_ext_cmd(sizeof(android_l_aid_matching_mode_on_cmd),
                                android_l_aid_matching_mode_on_cmd);
                if (status != NFCSTATUS_SUCCESS) {
                    NXPLOG_NCIHAL_E("Android L AID Matching Platform Setting Failed");
                    retry_core_init_cnt++;
                    goto retry_core_init;
                }
            }
        }
    }

    NXPLOG_NCIHAL_E("Resetting FW Dnld flag");
    fw_dwnld_flag = 0x00;

#ifdef FW_DWNLD_FLAG
    mEEPROM_info.buffer = &fw_dwnld_flag;
    mEEPROM_info.bufflen = sizeof(fw_dwnld_flag);
    mEEPROM_info.request_type = EEPROM_FW_DWNLD;
    mEEPROM_info.request_mode = SET_EEPROM_DATA;
    status = request_EEPROM(&mEEPROM_info);
    if (status == NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("Resetting FW Dnld flag SUCCESS");
    } else {
      NXPLOG_NCIHAL_E("Resetting FW Dnld flag FAILED");
    }
#endif
  }

  config_access = false;
  if (!((*p_core_init_rsp_params > 0) && (*p_core_init_rsp_params < 4))) {
      if(nfcFL.nfcNxpEse == true) {
          status = phNxpNciHal_check_eSE_Session_Identity();
          if (status != NFCSTATUS_SUCCESS) {
              NXPLOG_NCIHAL_E("Session id/ SWP intf reset Failed");
              retry_core_init_cnt++;
              goto retry_core_init;
          }
      }
    status = phNxpNciHal_send_ext_cmd(sizeof(cmd_reset_nci), cmd_reset_nci);
    if (status == NFCSTATUS_SUCCESS) {
        if (nxpncihal_ctrl.nci_info.nci_version == NCI_VERSION_2_0) {
          status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci2_0), cmd_init_nci2_0);
        } else {
          status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci), cmd_init_nci);
        }
        if (status != NFCSTATUS_SUCCESS)
            return status;
    } else {
      return NFCSTATUS_FAILED;
    }
    status = phNxpNciHal_send_get_cfgs();
    if (status == NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("Send get Configs SUCCESS");
    } else {
      NXPLOG_NCIHAL_E("Send get Configs FAILED");
    }
  }

  if(nfcFL.eseFL._WIRED_MODE_STANDBY == true) {
      if (nxpncihal_ctrl.hal_boot_mode == NFC_OSU_BOOT_MODE) {
          status = phNxpNciHal_send_nfcee_pwr_cntl_cmd(POWER_ALWAYS_ON);
          if (status == NFCSTATUS_SUCCESS) {
              NXPLOG_NCIHAL_E("Send nfcee_pwrcntl cmd SUCCESS");
          } else {
              NXPLOG_NCIHAL_E("Send nfcee_pwrcntl cmd FAILED");
          }
      }

      if (pwr_link_required == true) {
          phNxpNciHal_send_nfcee_pwr_cntl_cmd(POWER_ALWAYS_ON | LINK_ALWAYS_ON);
          pwr_link_required = false;
      }
  }

  if ((*p_core_init_rsp_params > 0) && (*p_core_init_rsp_params < 4)) {
    static phLibNfc_Message_t msg;
    uint16_t tmp_len = 0;
    uint8_t uicc_set_mode[] = {0x22, 0x01, 0x02, 0x02, 0x01};
    uint8_t set_screen_state[] = {0x2F, 0x15, 01, 00};  // SCREEN ON
    uint8_t set_screen_state_nci2[] = {0x20,0x09,0x01,0x00};
    uint8_t nfcc_core_conn_create[] = {0x20, 0x04, 0x06, 0x03, 0x01,
                                       0x01, 0x02, 0x01, 0x01};
    uint8_t nfcc_mode_set_on[] = {0x22, 0x01, 0x02, 0x01, 0x01};
    if(nxpncihal_ctrl.nci_info.nci_version != NCI_VERSION_2_0)
   {
    NXPLOG_NCIHAL_E(
        "Sending DH and NFCC core connection command as raw packet!!");
    status = phNxpNciHal_send_ext_cmd(sizeof(nfcc_core_conn_create),
                                      nfcc_core_conn_create);

    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E(
          "Sending DH and NFCC core connection command as raw packet!! Failed");
      retry_core_init_cnt++;
      goto retry_core_init;
    }

    NXPLOG_NCIHAL_E("Sending DH and NFCC mode set as raw packet!!");
    status =
        phNxpNciHal_send_ext_cmd(sizeof(nfcc_mode_set_on), nfcc_mode_set_on);

    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("Sending DH and NFCC mode set as raw packet!! Failed");
      retry_core_init_cnt++;
      goto retry_core_init;
    }
   }

    // Set the proper screen state
    switch (p_core_init_rsp_params[255]) {
      case 0x0:
      case 0x8:
        NXPLOG_NCIHAL_E("Last Screen State Sent = 0x0");
        set_screen_state[3] = 0x00;  // SCREEN ON UNLOCKED (listen and poll
                                     // mode)
        break;
      case 0x2:
        NXPLOG_NCIHAL_E("Last Screen State Sent = 0x1");
        set_screen_state[3] = 0x01;  // SCREEN OFF LOCKED
        set_screen_state_nci2[3] = 0x03;
        break;
      case 0x4:
        NXPLOG_NCIHAL_E("Last Screen State Sent = 0x2");
        set_screen_state[3] = 0x02;  // SCREEN ON LOCKED (only listen mode)
        set_screen_state_nci2[3] = 0x02;
        break;
      case 1:
        set_screen_state_nci2[3] = 0x01;
        set_screen_state[3] = 0x02;  // SCREEN OFF UNLOCKED (only listen mode)
        break;
      default:
        NXPLOG_NCIHAL_E("Setting default as SCREEN ON UNLOCKED");
        set_screen_state[3] = 0x00;  // SCREEN ON UNLOCKED (listen and poll
                                     // mode)
        break;
    }

    if (*(p_core_init_rsp_params + 1) == 1)  // RF state is Discovery!!
    {
      NXPLOG_NCIHAL_E("Sending Set Screen ON State Command as raw packet!!");
      if(nxpncihal_ctrl.nci_info.nci_version != NCI_VERSION_2_0)
      {
      status =
          phNxpNciHal_send_ext_cmd(sizeof(set_screen_state), set_screen_state);
      }else
      {
          status =
              phNxpNciHal_send_ext_cmd(sizeof(set_screen_state_nci2), set_screen_state_nci2);
      }
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E(
            "Sending Set Screen ON State Command as raw packet!! Failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }

      NXPLOG_NCIHAL_E("Sending discovery as raw packet!!");
      status = phNxpNciHal_send_ext_cmd(p_core_init_rsp_params[2],
                                        (uint8_t*)&p_core_init_rsp_params[3]);
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("Sending discovery as raw packet Failed");
        retry_core_init_cnt++;
        goto retry_core_init;
      }
    } else {
      NXPLOG_NCIHAL_E("Sending Set Screen OFF State Command as raw packet!!");
      if(nxpncihal_ctrl.nci_info.nci_version != NCI_VERSION_2_0)
      {
      status =
          phNxpNciHal_send_ext_cmd(sizeof(set_screen_state), set_screen_state);
      }else
      {
          status =
              phNxpNciHal_send_ext_cmd(sizeof(set_screen_state_nci2), set_screen_state_nci2);
      }
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E(
            "Sending Set Screen OFF State Command as raw packet!! Failed");
        retry_core_init_cnt++;
        goto retry_core_init;
        }
      }

    if (nxpprofile_ctrl.profile_type == EMV_CO_PROFILE) {
      NXPLOG_NCIHAL_E(
          "Current Profile : EMV_CO_PROFILE. Resetting to "
          "NFC_FORUM_PROFILE...");
      nxpprofile_ctrl.profile_type = NFC_FORUM_PROFILE;
    }

    NXPLOG_NCIHAL_E("Sending last command for Recovery ");
    /*
     * Field 35 will have total length of command HDR(2) + Length(1) + Cmd data(n).
     * - 36,37 - HDR(2).
     * - 38    - Length(1).
     * If length of last command is 0 then it doesn't need to send last command.
     * Field 35 - 38 will be 0x03 (HDR(2) + Length(1)) in all valid cases.
     * This extra check is added to avoid sending corrupt data as command.
     */
    if ((p_core_init_rsp_params[35] > 0)&&
            (p_core_init_rsp_params[35] - p_core_init_rsp_params[38] == 0x03)) {
      if (!((p_core_init_rsp_params[36] == 0x21) &&
            (p_core_init_rsp_params[37] == 0x03) &&
            (*(p_core_init_rsp_params + 1) == 0x01)) &&
          !((p_core_init_rsp_params[36] == 0x21) &&
            (p_core_init_rsp_params[37] == 0x06) &&
            (p_core_init_rsp_params[39] == 0x00) &&
            (*(p_core_init_rsp_params + 1) == 0x00)))
      // if last command is discovery and RF status is also discovery state,
      // then it doesn't need to execute or similarly
      // if the last command is deactivate to idle and RF status is also idle ,
      // no need to execute the command .
      {
        tmp_len = p_core_init_rsp_params[35];

        /* Check for NXP ext before sending write */
        status = phNxpNciHal_write_ext(
            &tmp_len, (uint8_t*)&p_core_init_rsp_params[36],
            &nxpncihal_ctrl.rsp_len, nxpncihal_ctrl.p_rsp_data);
        if (status != NFCSTATUS_SUCCESS) {
          if (buffer) {
            free(buffer);
            buffer = NULL;
          }
          /* Do not send packet to PN54X, send response directly */
          msg.eMsgType = NCI_HAL_RX_MSG;
          msg.pMsgData = NULL;
          msg.Size = 0;

          phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId,
                                (phLibNfc_Message_t*)&msg);
          return NFCSTATUS_SUCCESS;
        }

        p_core_init_rsp_params[35] = (uint8_t)tmp_len;

        status = phNxpNciHal_send_ext_cmd(
            p_core_init_rsp_params[35], (uint8_t*)&p_core_init_rsp_params[36]);
        if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("Sending last command for Recovery Failed");
          retry_core_init_cnt++;
          goto retry_core_init;
        }
      }
    }
  }

  retry_core_init_cnt = 0;

  if (buffer) {
    free(buffer);
    buffer = NULL;
  }
  config_access = false;

  if(nfcFL.chipType != pn547C2) {
  // initialize dummy FW recovery variables
  gRecFWDwnld = false;
  gRecFwRetryCount = 0;
}
  if (!((*p_core_init_rsp_params > 0) && (*p_core_init_rsp_params < 4)))
    phNxpNciHal_core_initialized_complete(status);
  else {
  invoke_callback:
    config_access = false;
    if (nxpncihal_ctrl.p_nfc_stack_data_cback != NULL) {
      *p_core_init_rsp_params = 0;
      NXPLOG_NCIHAL_E("Invoking data callback!!");
      (*nxpncihal_ctrl.p_nfc_stack_data_cback)(nxpncihal_ctrl.rx_data_len,
                                               nxpncihal_ctrl.p_rx_data);
    }
  }
  /* This code is moved to JNI
  #ifdef PN547C2_CLOCK_SETTING
      if (isNxpConfigModified())
      {
          updateNxpConfigTimestamp();
      }
  #endif*/
  return NFCSTATUS_SUCCESS;
}
/******************************************************************************
 * Function         phNxpNciHal_check_eSE_Session_Identity
 *
 * Description      This function is called at init time to check
 *                  the presence of ese related info and disable SWP interfaces.
 *
 * Returns          void.
 *
 ******************************************************************************/
static NFCSTATUS phNxpNciHal_check_eSE_Session_Identity(void) {
  struct stat st;
  int ret = 0;
  NFCSTATUS status = NFCSTATUS_FAILED;
  const char config_eseinfo_path[] = "/data/vendor/nfc/nfaStorage.bin1";
  static uint8_t session_identity[8] = {0x00};
  uint8_t default_session[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t swp2_intf_status = 0x00;
  long retlen = 0;
  static uint8_t disable_dual_swp_intf[] = {0x20, 0x02, 0x09, 0x02, 0xA0, 0xEC,
                                       0x01, 0x00, 0xA0, 0xD4, 0x01, 0x00};
  static uint8_t disable_swp_intf[] = {0x20, 0x02, 0x05, 0x01,
                                       0xA0, 0xEC, 0x01, 0x00};

  phNxpNci_EEPROM_info_t swp_intf_info;
  uint8_t swp_info_buff[32] = {0};

  memset(&swp_intf_info, 0, sizeof(swp_intf_info));
  swp_intf_info.request_mode = GET_EEPROM_DATA;
  swp_intf_info.request_type = EEPROM_SWP2_INTF;
  swp_intf_info.buffer = &swp2_intf_status;
  swp_intf_info.bufflen = sizeof(uint8_t);
  status = request_EEPROM(&swp_intf_info);
  NXPLOG_NCIHAL_D("%s swp2_intf_status = 0x%02X", __func__, swp2_intf_status);
  if ((status == NFCSTATUS_OK) && (swp2_intf_status == 0x00)) {
    pwr_link_required = false;
    return NFCSTATUS_SUCCESS;
  }

  if (stat(config_eseinfo_path, &st) == -1) {
    status = NFCSTATUS_FAILED;
    NXPLOG_NCIHAL_D("%s file not present = %s", __func__, config_eseinfo_path);
  } else {
    phNxpNci_EEPROM_info_t mEEPROM_info = {.request_mode = 0};
    mEEPROM_info.request_mode = GET_EEPROM_DATA;
    mEEPROM_info.request_type = EEPROM_ESE_SESSION_ID;
    mEEPROM_info.buffer = session_identity;
    mEEPROM_info.bufflen = sizeof(session_identity);
    status = request_EEPROM(&mEEPROM_info);
    if (status == NFCSTATUS_OK) {
      if (!memcmp((uint8_t*)session_identity, (uint8_t*)default_session,
                  sizeof(session_identity))) {
        status = NFCSTATUS_FAILED;
      } else {
        status = NFCSTATUS_OK;
      }
    }
  }

  if (status == NFCSTATUS_FAILED) {
    /*Disable SWP1 and 1A interfaces*/
      if(nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH) {
          status = phNxpNciHal_send_ext_cmd(sizeof(disable_dual_swp_intf),
                  disable_dual_swp_intf);
      }
      else {
          status = phNxpNciHal_send_ext_cmd(sizeof(disable_swp_intf),
                  disable_swp_intf);
      }
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NXP disable SWP interface_set command failed");
    }
    pwr_link_required = true;
  }
  return status;
}

/******************************************************************************
 * Function         phNxpNciHal_CheckRFCmdRespStatus
 *
 * Description      This function is called to check the resp status of
 *                  RF update commands.
 *
 * Returns          NFCSTATUS_SUCCESS           if successful,
 *                  NFCSTATUS_INVALID_PARAMETER if parameter is inavlid
 *                  NFCSTATUS_FAILED            if failed response
 *
 ******************************************************************************/
NFCSTATUS phNxpNciHal_CheckRFCmdRespStatus() {
    NXPLOG_NCIHAL_D("phNxpNciHal_CheckRFCmdRespStatus () Enter");
    if(nfcFL.chipType == pn547C2) {
        NXPLOG_NCIHAL_D("chipType : pn547C2. Not allowed. Returning");
        return NFCSTATUS_FAILED;
    }
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  static uint16_t INVALID_PARAM = 0x09;
  if ((nxpncihal_ctrl.rx_data_len > 0) && (nxpncihal_ctrl.p_rx_data[2] > 0)) {
    if (nxpncihal_ctrl.p_rx_data[3] == 0x09) {
      status = INVALID_PARAM;
    } else if (nxpncihal_ctrl.p_rx_data[3] != NFCSTATUS_SUCCESS) {
      status = NFCSTATUS_FAILED;
    }
  }
  return status;
}

/******************************************************************************
 * Function         phNxpNciHalRFConfigCmdRecSequence
 *
 * Description      This function is called to handle dummy FW recovery sequence
 *                  Whenever RF settings are failed to apply with invalid param
 *                  response , recovery mechanism  includes dummy firmware
 *download
 *                  followed by irmware downlaod and then config settings. The
 *dummy
 *                  firmware changes the major number of the firmware inside
 *NFCC.
 *                  Then actual firmware dowenload will be successful.This can
 *be
 *                  retried maximum three times.
 *
 * Returns          Always returns NFCSTATUS_SUCCESS
 *
 ******************************************************************************/
NFCSTATUS phNxpNciHalRFConfigCmdRecSequence() {
    NXPLOG_NCIHAL_D("phNxpNciHalRFConfigCmdRecSequence () Enter");
    if(nfcFL.chipType == pn547C2) {
        NXPLOG_NCIHAL_D("chipType : pn547C2. Not allowed. Returning");
        return NFCSTATUS_FAILED;
    }
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  uint16_t recFWState = 1;
  if(nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD == true) {
      gRecFWDwnld = false;
      force_fw_download_req = true;
  } else {
      gRecFWDwnld = true;
  }
  gRecFwRetryCount++;
  if (gRecFwRetryCount > 0x03) {
    NXPLOG_NCIHAL_D("Max retry count for RF config FW recovery exceeded ");
    gRecFWDwnld = false;
    if(nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD == true) {
        force_fw_download_req = false;
    }
    return NFCSTATUS_FAILED;
  }
  do {
    phDnldNfc_InitImgInfo();
    if (NFCSTATUS_SUCCESS == phNxpNciHal_CheckValidFwVersion()) {
      status = phNxpNciHal_fw_download();
      if (status == NFCSTATUS_SUCCESS) {
        status = phTmlNfc_Read(
            nxpncihal_ctrl.p_cmd_data, NCI_MAX_DATA_LEN,
            (pphTmlNfc_TransactCompletionCb_t)&phNxpNciHal_read_complete, NULL);
        if (status != NFCSTATUS_PENDING) {
          NXPLOG_NCIHAL_E("TML Read status error status = %x", status);
          status = phTmlNfc_Shutdown();
          status = NFCSTATUS_FAILED;
          break;
        }
      } else {
        status = NFCSTATUS_FAILED;
        break;
      }
    }
    gRecFWDwnld = false;
  } while (recFWState--);
  gRecFWDwnld = false;
  if(nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD == true) {
      force_fw_download_req = false;
  }
  return status;
}

/******************************************************************************
 * Function         phNxpNciHal_uicc_baud_rate
 *
 * Description      This function is used to restrict the UICC baud
 *                  rate for type A and type B UICC.
 *
 * Returns          Status.
 *
 ******************************************************************************/
static NFCSTATUS phNxpNciHal_uicc_baud_rate() {
    NXPLOG_NCIHAL_D("phNxpNciHal_uicc_baud_rate () Enter");
    if(nfcFL.chipType != pn547C2) {
        NXPLOG_NCIHAL_D("chipType != pn547C2. Not allowed. Returning");
        return NFCSTATUS_FAILED;
    }
  unsigned long configValue = 0x00;
  uint16_t bitRateCmdLen = 0x04;  // HDR + LEN + PARAMS   2 + 1 + 1
  uint8_t uiccTypeAValue = 0x00;  // read uicc type A value
  uint8_t uiccTypeBValue = 0x00;  // read uicc type B value
  uint8_t setUiccBitRateBuf[] = {0x20, 0x02, 0x01, 0x00, 0xA0, 0x86,
                                 0x01, 0x91, 0xA0, 0x87, 0x01, 0x91};
  uint8_t getUiccBitRateBuf[] = {0x20, 0x03, 0x05, 0x02,
                                 0xA0, 0x86, 0xA0, 0x87};
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  status =
      phNxpNciHal_send_ext_cmd(sizeof(getUiccBitRateBuf), getUiccBitRateBuf);
  if (status == NFCSTATUS_SUCCESS && nxpncihal_ctrl.rx_data_len >= 0x0D) {
    if (nxpncihal_ctrl.p_rx_data[0] == 0x40 &&
        nxpncihal_ctrl.p_rx_data[1] == 0x03 &&
        nxpncihal_ctrl.p_rx_data[2] > 0x00 &&
        nxpncihal_ctrl.p_rx_data[3] == 0x00) {
      uiccTypeAValue = nxpncihal_ctrl.p_rx_data[8];
      uiccTypeBValue = nxpncihal_ctrl.p_rx_data[12];
    }
  }
  /* NXP Restrict Type A UICC baud rate */
  if (GetNxpNumValue(NAME_NXP_TYPEA_UICC_BAUD_RATE, (void*)&configValue,
                     sizeof(configValue))) {
    if (configValue == 0x00) {
      NXPLOG_NCIHAL_D("Default UICC TypeA Baud Rate supported");
    } else {
      setUiccBitRateBuf[2] += 0x04;  // length byte
      setUiccBitRateBuf[3] = 0x01;   // param byte
      bitRateCmdLen += 0x04;
      if (configValue == 0x01 && uiccTypeAValue != 0x91) {
        NXPLOG_NCIHAL_D("UICC TypeA Baud Rate 212kbps supported");
        setUiccBitRateBuf[7] = 0x91;  // set config value for 212
      } else if (configValue == 0x02 && uiccTypeAValue != 0xB3) {
        NXPLOG_NCIHAL_D("UICC TypeA Baud Rate 424kbps supported");
        setUiccBitRateBuf[7] = 0xB3;  // set config value for 424
      } else if (configValue == 0x03 && uiccTypeAValue != 0xF7) {
        NXPLOG_NCIHAL_D("UICC TypeA Baud Rate 848kbps supported");
        setUiccBitRateBuf[7] = 0xF7;  // set config value for 848
      } else {
        setUiccBitRateBuf[3] = 0x00;
        setUiccBitRateBuf[2] -= 0x04;
        bitRateCmdLen -= 0x04;
      }
    }
  }
  configValue = 0;
  /* NXP Restrict Type B UICC baud rate*/
  if (GetNxpNumValue(NAME_NXP_TYPEB_UICC_BAUD_RATE, (void*)&configValue,
                     sizeof(configValue))) {
    if (configValue == 0x00) {
      NXPLOG_NCIHAL_D("Default UICC TypeB Baud Rate supported");
    } else {
      setUiccBitRateBuf[2] += 0x04;
      setUiccBitRateBuf[3] += 0x01;
      setUiccBitRateBuf[bitRateCmdLen++] = 0xA0;
      setUiccBitRateBuf[bitRateCmdLen++] = 0x87;
      setUiccBitRateBuf[bitRateCmdLen++] = 0x01;
      if (configValue == 0x01 && uiccTypeBValue != 0x91) {
        NXPLOG_NCIHAL_D("UICC TypeB Baud Rate 212kbps supported");
        setUiccBitRateBuf[bitRateCmdLen++] = 0x91;  // set config value for 212
      } else if (configValue == 0x02 && uiccTypeBValue != 0xB3) {
        NXPLOG_NCIHAL_D("UICC TypeB Baud Rate 424kbps supported");
        setUiccBitRateBuf[bitRateCmdLen++] = 0xB3;  // set config value for 424
      } else if (configValue == 0x03 && uiccTypeBValue != 0xF7) {
        NXPLOG_NCIHAL_D("UICC TypeB Baud Rate 848kbps supported");
        setUiccBitRateBuf[bitRateCmdLen++] = 0xF7;  // set config value for 848
      } else {
        setUiccBitRateBuf[2] -= 0x04;
        setUiccBitRateBuf[3] -= 0x01;
        bitRateCmdLen -= 0x04;
      }
    }
  }
  if (bitRateCmdLen > 0x04) {
    status = phNxpNciHal_send_ext_cmd(bitRateCmdLen, setUiccBitRateBuf);
  }
  return status;
}

/******************************************************************************
 * Function         phNxpNciHal_core_initialized_complete
 *
 * Description      This function is called when phNxpNciHal_core_initialized
 *                  complete all proprietary command exchanges. This function
 *                  informs libnfc-nci about completion of core initialize
 *                  and result of that through callback.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_core_initialized_complete(NFCSTATUS status) {
  static phLibNfc_Message_t msg;

  if (status == NFCSTATUS_SUCCESS) {
    msg.eMsgType = NCI_HAL_POST_INIT_CPLT_MSG;
  } else {
    msg.eMsgType = NCI_HAL_ERROR_MSG;
  }
  msg.pMsgData = NULL;
  msg.Size = 0;

  phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId,
                        (phLibNfc_Message_t*)&msg);

  return;
}
/******************************************************************************
 * Function         phNxpNciHal_core_MinInitialized_complete
 *
 * Description      This function is called when phNxpNciHal_core_initialized
 *                  complete all proprietary command exchanges. This function
 *                  informs libnfc-nci about completion of core initialize
 *                  and result of that through callback.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_core_MinInitialized_complete(NFCSTATUS status) {
  static phLibNfc_Message_t msg;

  if (status == NFCSTATUS_SUCCESS) {
    msg.eMsgType = NCI_HAL_POST_MIN_INIT_CPLT_MSG;
  } else {
    msg.eMsgType = NCI_HAL_ERROR_MSG;
  }
  msg.pMsgData = NULL;
  msg.Size = 0;

  phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId,
                        (phLibNfc_Message_t*)&msg);

  return;
}
/******************************************************************************
 * Function         phNxpNciHal_pre_discover
 *
 * Description      This function is called by libnfc-nci to perform any
 *                  proprietary exchange before RF discovery. When proprietary
 *                  exchange is over completion is informed to libnfc-nci
 *                  through phNxpNciHal_pre_discover_complete function.
 *
 * Returns          It always returns NFCSTATUS_SUCCESS (0).
 *
 ******************************************************************************/
int phNxpNciHal_pre_discover(void) {
  /* Nothing to do here for initial version */
  return NFCSTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpNciHal_pre_discover_complete
 *
 * Description      This function informs libnfc-nci about completion and
 *                  status of phNxpNciHal_pre_discover through callback.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_pre_discover_complete(NFCSTATUS status) {
  static phLibNfc_Message_t msg;

  if (status == NFCSTATUS_SUCCESS) {
    msg.eMsgType = NCI_HAL_PRE_DISCOVER_CPLT_MSG;
  } else {
    msg.eMsgType = NCI_HAL_ERROR_MSG;
  }
  msg.pMsgData = NULL;
  msg.Size = 0;

  phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId, &msg);

  return;
}

/******************************************************************************
 * Function         phNxpNciHal_release_info
 *
 * Description      This function frees allocated memory for mGetCfg_info
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_release_info(void) {
  NXPLOG_NCIHAL_D("phNxpNciHal_release_info mGetCfg_info");
  if (mGetCfg_info != NULL) {
    free(mGetCfg_info);
    mGetCfg_info = NULL;
  }
}

/******************************************************************************
 * Function         phNxpNciHal_close
 *
 * Description      This function close the NFCC interface and free all
 *                  resources.This is called by libnfc-nci on NFC service stop.
 *
 * Returns          Always return NFCSTATUS_SUCCESS (0).
 *
 ******************************************************************************/
int phNxpNciHal_close(void) {
  NFCSTATUS status = NFCSTATUS_FAILED;
  uint8_t cmd_ce_discovery_nci[10] = {
      0x21, 0x03,
  };
  static uint8_t cmd_core_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  uint8_t length = 0;
  uint8_t numPrms = 0;
  uint8_t ptr = 4;
  unsigned long uiccListenMask = 0x00;

  if (!(GetNxpNumValue(NAME_NXP_UICC_LISTEN_TECH_MASK, &uiccListenMask,
                       sizeof(uiccListenMask)))) {
    uiccListenMask = 0x07;
    NXPLOG_NCIHAL_D("UICC_LISTEN_TECH_MASK = 0x%0lX", uiccListenMask);
  }

  if (nxpncihal_ctrl.hal_boot_mode == NFC_FAST_BOOT_MODE) {
    NXPLOG_NCIHAL_E(" HAL NFC fast init mode calling min_close %d",
                    nxpncihal_ctrl.hal_boot_mode);
    status = phNxpNciHal_Minclose();
    return status;
  }

  if (nxpncihal_ctrl.halStatus == HAL_STATUS_CLOSE) {
    NXPLOG_NCIHAL_E("phNxpNciHal_close is already closed, ignoring close");
    return NFCSTATUS_FAILED;
  }

  CONCURRENCY_LOCK();
  if (nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT &&
          read_failed_disable_nfc) {
      read_failed_disable_nfc = false;
      goto close_and_return;
  }

  if (write_unlocked_status == NFCSTATUS_FAILED) {
    NXPLOG_NCIHAL_D("phNxpNciHal_close i2c write failed .Clean and Return");
    goto close_and_return;
  }

  if ((uiccListenMask & 0x1) == 0x01) {
    NXPLOG_NCIHAL_D("phNxpNciHal_close (): Adding A passive listen");
    numPrms++;
    cmd_ce_discovery_nci[ptr++] = 0x80;
    cmd_ce_discovery_nci[ptr++] = 0x01;
    length += 2;
  }
  if ((uiccListenMask & 0x2) == 0x02) {
    NXPLOG_NCIHAL_D("phNxpNciHal_close (): Adding B passive listen");
    numPrms++;
    cmd_ce_discovery_nci[ptr++] = 0x81;
    cmd_ce_discovery_nci[ptr++] = 0x01;
    length += 2;
  }
  if ((uiccListenMask & 0x4) == 0x04) {
    NXPLOG_NCIHAL_D("phNxpNciHal_close (): Adding F passive listen");
    numPrms++;
    cmd_ce_discovery_nci[ptr++] = 0x82;
    cmd_ce_discovery_nci[ptr++] = 0x01;
    length += 2;
  }

  if (length != 0) {
    cmd_ce_discovery_nci[2] = length + 1;
    cmd_ce_discovery_nci[3] = numPrms;
    status = phNxpNciHal_send_ext_cmd(length + 4, cmd_ce_discovery_nci);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("CMD_CE_DISC_NCI: Failed");
    }
  } else {
    NXPLOG_NCIHAL_E(
        "No changes in the discovery command, sticking to last discovery "
        "command sent");
  }

  nxpncihal_ctrl.halStatus = HAL_STATUS_CLOSE;

  status =
      phNxpNciHal_send_ext_cmd(sizeof(cmd_core_reset_nci), cmd_core_reset_nci);
  if (status != NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_E("NCI_CORE_RESET: Failed");
  }
close_and_return:

  if (NULL != gpphTmlNfc_Context->pDevHandle) {
    phNxpNciHal_close_complete(NFCSTATUS_SUCCESS);
    /* Abort any pending read and write */
    status = phTmlNfc_ReadAbort();

    status = phTmlNfc_WriteAbort();

    phOsalNfc_Timer_Cleanup();

    status = phTmlNfc_Shutdown();

    phDal4Nfc_msgrelease(nxpncihal_ctrl.gDrvCfg.nClientId);

    memset(&nxpncihal_ctrl, 0x00, sizeof(nxpncihal_ctrl));

    NXPLOG_NCIHAL_D("phNxpNciHal_close - phOsalNfc_DeInit completed");
  }

  CONCURRENCY_UNLOCK();

  phNxpNciHal_cleanup_monitor();
  write_unlocked_status = NFCSTATUS_SUCCESS;
  phNxpNciHal_release_info();
  /* Return success always */
  return NFCSTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpNciHal_Minclose
 *
 * Description      This function close the NFCC interface and free all
 *                  resources.This is called by libnfc-nci on NFC service stop.
 *
 * Returns          Always return NFCSTATUS_SUCCESS (0).
 *
 ******************************************************************************/
int phNxpNciHal_Minclose(void) {
  NFCSTATUS status;
  /*NCI_RESET_CMD*/
  uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  CONCURRENCY_LOCK();
  nxpncihal_ctrl.halStatus = HAL_STATUS_CLOSE;
  status = phNxpNciHal_send_ext_cmd(sizeof(cmd_reset_nci), cmd_reset_nci);
  if (status != NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_E("NCI_CORE_RESET: Failed");
  }
  if (NULL != gpphTmlNfc_Context->pDevHandle) {
    phNxpNciHal_close_complete(NFCSTATUS_SUCCESS);
    /* Abort any pending read and write */
    status = phTmlNfc_ReadAbort();
    status = phTmlNfc_WriteAbort();

    phOsalNfc_Timer_Cleanup();

    status = phTmlNfc_Shutdown();

    phDal4Nfc_msgrelease(nxpncihal_ctrl.gDrvCfg.nClientId);

    memset(&nxpncihal_ctrl, 0x00, sizeof(nxpncihal_ctrl));

    NXPLOG_NCIHAL_D("phNxpNciHal_close - phOsalNfc_DeInit completed");
  }

  CONCURRENCY_UNLOCK();

  phNxpNciHal_cleanup_monitor();

  /* Return success always */
  return NFCSTATUS_SUCCESS;
}
/******************************************************************************
 * Function         phNxpNciHal_close_complete
 *
 * Description      This function inform libnfc-nci about result of
 *                  phNxpNciHal_close.
 *
 * Returns          void.
 *
 ******************************************************************************/
void phNxpNciHal_close_complete(NFCSTATUS status) {
  static phLibNfc_Message_t msg;

  if (status == NFCSTATUS_SUCCESS) {
    msg.eMsgType = NCI_HAL_CLOSE_CPLT_MSG;
  } else {
    msg.eMsgType = NCI_HAL_ERROR_MSG;
  }
  msg.pMsgData = NULL;
  msg.Size = 0;

  phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId, &msg);

  return;
}
/******************************************************************************
 * Function         phNxpNciHal_notify_i2c_fragmentation
 *
 * Description      This function can be used by HAL to inform
 *                 libnfc-nci that i2c fragmentation is enabled/disabled
 *
 * Returns          void.
 *
 ******************************************************************************/
void phNxpNciHal_notify_i2c_fragmentation(void) {
  if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
    /*inform libnfc-nci that i2c fragmentation is enabled/disabled */
    (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT,
                                        HAL_NFC_STATUS_OK);
  }
}
/******************************************************************************
 * Function         phNxpNciHal_control_granted
 *
 * Description      Called by libnfc-nci when NFCC control is granted to HAL.
 *
 * Returns          Always returns NFCSTATUS_SUCCESS (0).
 *
 ******************************************************************************/
int phNxpNciHal_control_granted(void) {
  /* Take the concurrency lock so no other calls from upper layer
   * will be allowed
   */
  CONCURRENCY_LOCK();

  if (NULL != nxpncihal_ctrl.p_control_granted_cback) {
    (*nxpncihal_ctrl.p_control_granted_cback)();
  }
  /* At the end concurrency unlock so calls from upper layer will
   * be allowed
   */
  CONCURRENCY_UNLOCK();
  return NFCSTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpNciHal_request_control
 *
 * Description      This function can be used by HAL to request control of
 *                  NFCC to libnfc-nci. When control is provided to HAL it is
 *                  notified through phNxpNciHal_control_granted.
 *
 * Returns          void.
 *
 ******************************************************************************/
void phNxpNciHal_request_control(void) {
  if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
    /* Request Control of NCI Controller from NCI NFC Stack */
    (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_REQUEST_CONTROL_EVT,
                                        HAL_NFC_STATUS_OK);
  }

  return;
}

/******************************************************************************
 * Function         phNxpNciHal_release_control
 *
 * Description      This function can be used by HAL to release the control of
 *                  NFCC back to libnfc-nci.
 *
 * Returns          void.
 *
 ******************************************************************************/
void phNxpNciHal_release_control(void) {
  if (nxpncihal_ctrl.p_nfc_stack_cback != NULL) {
    /* Release Control of NCI Controller to NCI NFC Stack */
    (*nxpncihal_ctrl.p_nfc_stack_cback)(HAL_NFC_RELEASE_CONTROL_EVT,
                                        HAL_NFC_STATUS_OK);
  }

  return;
}

/******************************************************************************
 * Function         phNxpNciHal_power_cycle
 *
 * Description      This function is called by libnfc-nci when power cycling is
 *                  performed. When processing is complete it is notified to
 *                  libnfc-nci through phNxpNciHal_power_cycle_complete.
 *
 * Returns          Always return NFCSTATUS_SUCCESS (0).
 *
 ******************************************************************************/
int phNxpNciHal_power_cycle(void) {
  NXPLOG_NCIHAL_D("Power Cycle");
  NFCSTATUS status = NFCSTATUS_FAILED;
  if (nxpncihal_ctrl.halStatus != HAL_STATUS_OPEN) {
    NXPLOG_NCIHAL_D("Power Cycle failed due to hal status not open");
    return NFCSTATUS_FAILED;
  }
  status = phTmlNfc_IoCtl(phTmlNfc_e_ResetDevice);

  if (NFCSTATUS_SUCCESS == status) {
    NXPLOG_NCIHAL_D("PN54X Reset - SUCCESS\n");
  } else {
    NXPLOG_NCIHAL_D("PN54X Reset - FAILED\n");
  }

  phNxpNciHal_power_cycle_complete(NFCSTATUS_SUCCESS);
  return NFCSTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpNciHal_power_cycle_complete
 *
 * Description      This function is called to provide the status of
 *                  phNxpNciHal_power_cycle to libnfc-nci through callback.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_power_cycle_complete(NFCSTATUS status) {
  static phLibNfc_Message_t msg;

  if (status == NFCSTATUS_SUCCESS) {
    msg.eMsgType = NCI_HAL_OPEN_CPLT_MSG;
  } else {
    msg.eMsgType = NCI_HAL_ERROR_MSG;
  }
  msg.pMsgData = NULL;
  msg.Size = 0;

  phTmlNfc_DeferredCall(gpphTmlNfc_Context->dwCallbackThreadId, &msg);

  return;
}

/******************************************************************************
 * Function         phNxpNciHal_ioctl
 *
 * Description      This function is called by jni when wired mode is
 *                  performed.First Pn54x driver will give the access
 *                  permission whether wired mode is allowed or not
 *                  arg (0):
 * Returns          return 0 on success and -1 on fail, On success
 *                  update the acutual state of operation in arg pointer
 *
 ******************************************************************************/
int phNxpNciHal_ioctl(long arg, void* p_data) {
  NXPLOG_NCIHAL_D("%s : enter - arg = %ld", __func__, arg);
  nfc_nci_IoctlInOutData_t* pInpOutData = p_data;
  int ret = -1;
  NFCSTATUS status = NFCSTATUS_FAILED;
  phNxpNciHal_FwRfupdateInfo_t* FwRfInfo;
  NFCSTATUS fm_mw_ver_check = NFCSTATUS_FAILED;

  switch (arg) {
    case HAL_NFC_IOCTL_P61_IDLE_MODE:
        if(nfcFL.nfcNxpEse) {
            status = phTmlNfc_IoCtl(phTmlNfc_e_SetP61IdleMode);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_P61_WIRED_MODE:
        if(nfcFL.nfcNxpEse) {
            status = phTmlNfc_IoCtl(phTmlNfc_e_SetP61WiredMode);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_P61_PWR_MODE:
        if(nfcFL.nfcNxpEse) {
            status = phTmlNfc_IoCtl(phTmlNfc_e_GetP61PwrMode);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data)
                    pInpOutData->out.data.p61CurrentState = (uint16_t)status;
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_P61_ENABLE_MODE:
        if(nfcFL.nfcNxpEse) {
            status = phTmlNfc_IoCtl(phTmlNfc_e_SetP61EnableMode);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_P61_DISABLE_MODE:
        if(nfcFL.nfcNxpEse) {
            status = phTmlNfc_IoCtl(phTmlNfc_e_SetP61DisableMode);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_P61_GET_ACCESS:
        if(nfcFL.nfcNxpEse) {
            NXPLOG_NCIHAL_D("HAL_NFC_IOCTL_P61_GET_ACCESS timeoutInMillisec = %d",
                    pInpOutData->inp.data.timeoutMilliSec);
            status = phTmlNfc_get_ese_access(gpphTmlNfc_Context->pDevHandle,
                    pInpOutData->inp.data.timeoutMilliSec);
            if (NFCSTATUS_SUCCESS == status) {
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_P61_REL_ACCESS:
        if(nfcFL.nfcNxpEse) {
            status = phTmlNfc_IoCtl(phTmlNfc_e_RelP61Access);
            NXPLOG_NCIHAL_D("HAL_NFC_IOCTL_P61_REL_ACCESS retval = %d\n", status);
            pInpOutData->out.data.status = status;
            if (NFCSTATUS_SUCCESS == status) {
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_ESE_CHIP_RST:
        if(nfcFL.nfcNxpEse) {
            status = phTmlNfc_IoCtl(phTmlNfc_e_eSEChipRstMode);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_REL_SVDD_WAIT:
        if(nfcFL.nfcNxpEse && nfcFL.eseFL._ESE_SVDD_SYNC) {
            status = phTmlNfc_rel_svdd_wait(gpphTmlNfc_Context->pDevHandle);
            NXPLOG_NCIHAL_D("HAL_NFC_IOCTL_P61_REL_SVDD_WAIT retval = %d\n", status);
            pInpOutData->out.data.status = status;
            if (NFCSTATUS_SUCCESS == status) {
                ret = 0;
            }
        }
      break;

    case HAL_NFC_IOCTL_SET_BOOT_MODE:
      if (NULL != p_data) {
        status = phNxpNciHal_set_Boot_Mode(pInpOutData->inp.data.bootMode);
        if (NFCSTATUS_FAILED != status) {
          pInpOutData->out.data.status = (uint16_t)status;
          ret = 0;
        }
      }
      break;
    case HAL_NFC_IOCTL_SET_JCP_DWNLD_ENABLE:
        if(nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION) {
            NXPLOG_NCIHAL_D("HAL_NFC_IOCTL_SET_JCP_DWNLD_ENABLE: \n");
            status = phTmlNfc_IoCtl(phTmlNfc_e_SetJcopDwnldEnable);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
                ret = 0;
            }
        }
      break;
    case HAL_NFC_IOCTL_SET_JCP_DWNLD_DISABLE:
        if(nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION) {
            NXPLOG_NCIHAL_D("HAL_NFC_IOCTL_SET_JCP_DWNLD_DISABLE: \n");
            status = phTmlNfc_IoCtl(phTmlNfc_e_SetJcopDwnldDisable);
            if (NFCSTATUS_FAILED != status) {
                if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
                ret = 0;
            }
        }
      break;

    case HAL_NFC_IOCTL_GET_CONFIG_INFO:
      if (mGetCfg_info) {
        memcpy(pInpOutData->out.data.nxpNciAtrInfo, mGetCfg_info,
               sizeof(phNxpNci_getCfg_info_t));
      } else {
        NXPLOG_NCIHAL_E("%s : Error! mgetCfg_info is Empty ", __func__);
      }
      ret = 0;
      break;
    case HAL_NFC_IOCTL_CHECK_FLASH_REQ:
      FwRfInfo =
          (phNxpNciHal_FwRfupdateInfo_t*)&pInpOutData->out.data.fwUpdateInf;
      status = phNxpNciHal_CheckFwRegFlashRequired(&FwRfInfo->fw_update_reqd,
                                                   &FwRfInfo->rf_update_reqd);
      if (NFCSTATUS_SUCCESS == status) {
#ifndef FW_DWNLD_FLAG
        fw_dwnld_flag = FwRfInfo->fw_update_reqd;
#endif
        ret = 0;
      }
      break;
    case HAL_NFC_IOCTL_FW_DWNLD:
      status = phNxpNciHal_FwDwnld();
      pInpOutData->out.data.fwDwnldStatus = (uint16_t)status;
      if (NFCSTATUS_SUCCESS == status) {
        ret = 0;
      }
      if(nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD) {
          force_fw_download_req = false;
      }
      break;
    case HAL_NFC_IOCTL_FW_MW_VER_CHECK:
      fm_mw_ver_check = phNxpNciHal_fw_mw_ver_check();
      pInpOutData->out.data.fwMwVerStatus = fm_mw_ver_check;
      ret = 0;
      break;
    case HAL_NFC_IOCTL_NCI_TRANSCEIVE:
      if (p_data == NULL) {
        ret = -1;
        break;
      }

      if (pInpOutData->inp.data.nciCmd.cmd_len <= 0) {
        ret = -1;
        break;
      }

      ret = phNxpNciHal_send_ext_cmd(pInpOutData->inp.data.nciCmd.cmd_len,
                                     pInpOutData->inp.data.nciCmd.p_cmd);
      pInpOutData->out.data.nciRsp.rsp_len = nxpncihal_ctrl.rx_data_len;
      if ((nxpncihal_ctrl.rx_data_len > 0) &&
          (nxpncihal_ctrl.rx_data_len <= MAX_IOCTL_TRANSCEIVE_RESP_LEN) &&
          (nxpncihal_ctrl.p_rx_data != NULL)) {
        memcpy(pInpOutData->out.data.nciRsp.p_rsp, nxpncihal_ctrl.p_rx_data,
               nxpncihal_ctrl.rx_data_len);
      }
      break;
    case HAL_NFC_IOCTL_DISABLE_HAL_LOG:
      status = phNxpLog_EnableDisableLogLevel(pInpOutData->inp.data.halType);
      break;
    case HAL_NFC_IOCTL_SET_NFC_SERVICE_PID:
      gpphTmlNfc_Context->nfc_service_pid = pInpOutData->inp.data.nfcServicePid;
      status = phTmlNfc_IoCtl(phTmlNfc_e_SetNfcServicePid);
      if (NFCSTATUS_FAILED != status) {
        if (NULL != p_data) pInpOutData->out.data.status = (uint16_t)status;
        ret = 0;
      }
      break;
    case HAL_NFC_IOCTL_GET_FEATURE_LIST:
        pInpOutData->out.data.chipType = (uint8_t)phNxpNciHal_getChipType();
        ret = 0;
        break;
    default:
      NXPLOG_NCIHAL_E("%s : Wrong arg = %ld", __func__, arg);
      break;
  }
  NXPLOG_NCIHAL_D("%s : exit - ret = %d", __func__, ret);
  return ret;
}

/******************************************************************************
 * Function         phNxpNciHal_set_clock
 *
 * Description      This function is called after successfull download
 *                  to apply the clock setting provided in config file
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_set_clock(void) {
  NFCSTATUS status = NFCSTATUS_FAILED;
  int retryCount = 0;

retrySetclock:
  phNxpNciClock.isClockSet = true;
  if (nxpprofile_ctrl.bClkSrcVal == CLK_SRC_PLL) {
    static uint8_t set_clock_cmd[] = {0x20, 0x02, 0x09, 0x02, 0xA0, 0x03,
                                      0x01, 0x11, 0xA0, 0x04, 0x01, 0x01};
    uint8_t param_clock_src;

    if((nfcFL.chipType == pn553) || (nfcFL.chipType == pn557)) {
        param_clock_src = 0x00;
    }
    else {
        param_clock_src = CLK_SRC_PLL;
        param_clock_src = param_clock_src << 3;
    }

    if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_13MHZ) {
      param_clock_src |= 0x00;
    } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_19_2MHZ) {
      param_clock_src |= 0x01;
    } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_24MHZ) {
      param_clock_src |= 0x02;
    } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_26MHZ) {
      param_clock_src |= 0x03;
    } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_38_4MHZ) {
      param_clock_src |= 0x04;
    } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_52MHZ) {
      param_clock_src |= 0x05;
    } else {
      NXPLOG_NCIHAL_E("Wrong clock freq, send default PLL@19.2MHz");

      if((nfcFL.chipType == pn553) || (nfcFL.chipType == pn557)) {
          param_clock_src = 0x01;
      }
      else {
          param_clock_src = 0x11;
      }
    }

    set_clock_cmd[7] = param_clock_src;
    set_clock_cmd[11] = nxpprofile_ctrl.bTimeout;
    status = phNxpNciHal_send_ext_cmd(sizeof(set_clock_cmd), set_clock_cmd);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("PLL colck setting failed !!");
    }
  } else if (nxpprofile_ctrl.bClkSrcVal == CLK_SRC_XTAL) {
    static uint8_t set_clock_cmd[] = {0x20, 0x02, 0x05, 0x01,
                                      0xA0, 0x03, 0x01, 0x08};
    status = phNxpNciHal_send_ext_cmd(sizeof(set_clock_cmd), set_clock_cmd);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("XTAL colck setting failed !!");
    }
  } else {
    NXPLOG_NCIHAL_E("Wrong clock source. Dont apply any modification")
  }

  // Checking for SET CONFG SUCCESS, re-send the command  if not.
  phNxpNciClock.isClockSet = false;
  if (phNxpNciClock.p_rx_data[3] != NFCSTATUS_SUCCESS) {
    if (retryCount++ < 3) {
      NXPLOG_NCIHAL_E("Set-clk failed retry again ");
      goto retrySetclock;
    } else {
      NXPLOG_NCIHAL_D("Set clk  failed -  max count = 0x%x exceeded ",
                      retryCount);
      //            NXPLOG_NCIHAL_E("Set Config is failed for Clock Due to
      //            elctrical disturbances, aborting the NFC process");
      //            abort ();
    }
  }
}

/******************************************************************************
 * Function         phNxpNciHal_check_clock_config
 *
 * Description      This function is called after successfull download
 *                  to check if clock settings in config file and chip
 *                  is same
 *
 * Returns          void.
 *
 ******************************************************************************/
NFCSTATUS phNxpNciHal_check_clock_config(void) {
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  uint8_t param_clock_src;
  static uint8_t get_clock_cmd[] = {0x20, 0x03, 0x07, 0x03, 0xA0,
                                    0x02, 0xA0, 0x03, 0xA0, 0x04};
  phNxpNciClock.isClockSet = true;
  phNxpNciHal_get_clk_freq();
  status = phNxpNciHal_send_ext_cmd(sizeof(get_clock_cmd), get_clock_cmd);

  if (status != NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_E("unable to retrieve get_clk_src_sel");
    return status;
  }
  param_clock_src = check_config_parameter();
  if (phNxpNciClock.p_rx_data[12] == param_clock_src &&
      phNxpNciClock.p_rx_data[16] == nxpprofile_ctrl.bTimeout) {
    phNxpNciClock.issetConfig = false;
  } else {
    phNxpNciClock.issetConfig = true;
  }
  phNxpNciClock.isClockSet = false;

  return status;
}

/******************************************************************************
 * Function         phNxpNciHal_set_china_region_configs
 *
 * Description      This function is called to set china region specific configs
 *
 * Returns          Status.
 *
 ******************************************************************************/
NFCSTATUS phNxpNciHal_set_china_region_configs(void) {
  NFCSTATUS status = NFCSTATUS_SUCCESS;
  int isfound = 0;
  unsigned long rf_enable = false;
  unsigned long cfg_blk_chk_enable = false;
  int rf_val = 0;
  int flag_send_tianjin_config = true;
  int flag_send_transit_config = true;
  uint8_t retry_cnt = 0;
  int enable_bit = 0;
  int enable_blk_num_chk_bit = 0;
  static uint8_t get_rf_cmd[] = {0x20, 0x03, 0x03, 0x01, 0xA0, 0x85};

retry_send_ext:
  if (retry_cnt > 3) {
    return NFCSTATUS_FAILED;
  }

  phNxpNciRfSet.isGetRfSetting = true;
  status = phNxpNciHal_send_ext_cmd(sizeof(get_rf_cmd), get_rf_cmd);
  if (status != NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_E("unable to get the RF setting");
    phNxpNciRfSet.isGetRfSetting = false;
    retry_cnt++;
    goto retry_send_ext;
  }
  phNxpNciRfSet.isGetRfSetting = false;
  if (phNxpNciRfSet.p_rx_data[3] != 0x00) {
    NXPLOG_NCIHAL_E("GET_CONFIG_RSP is FAILED for CHINA TIANJIN");
    return status;
  }

  /* check if tianjin_rf_setting is required */
  rf_val = phNxpNciRfSet.p_rx_data[10];
  isfound = (GetNxpNumValue(NAME_NXP_CHINA_TIANJIN_RF_ENABLED,
                            (void*)&rf_enable, sizeof(rf_enable)));
  if (isfound > 0) {
      enable_bit = rf_val & 0x40;
      if(nfcFL.nfccFL._NFCC_MIFARE_TIANJIN) {
          if ((enable_bit != 0x40) && (rf_enable == 1)) {
              phNxpNciRfSet.p_rx_data[10] |= 0x40;  // Enable if it is disabled
          } else if ((enable_bit == 0x40) && (rf_enable == 0)) {
              phNxpNciRfSet.p_rx_data[10] &= 0xBF;  // Disable if it is Enabled
          } else {
              flag_send_tianjin_config = false;  // No need to change in RF setting
          }
      }
      else {
          enable_bit = phNxpNciRfSet.p_rx_data[11] & 0x10;
          if ((rf_enable == 1) && (enable_bit != 0x10)) {
              NXPLOG_NCIHAL_E("Setting Non-Mifare reader for china tianjin");
              phNxpNciRfSet.p_rx_data[11] |= 0x10;
          } else if ((rf_enable == 0) && (enable_bit == 0x10)) {
              NXPLOG_NCIHAL_E("Setting Non-Mifare reader for china tianjin");
              phNxpNciRfSet.p_rx_data[11] &= 0xEF;
          } else {
              flag_send_tianjin_config = false;
          }
      }
  }
  /*check if china block number check is required*/
  rf_val = phNxpNciRfSet.p_rx_data[8];
  isfound =
      (GetNxpNumValue(NAME_NXP_CHINA_BLK_NUM_CHK_ENABLE,
                      (void*)&cfg_blk_chk_enable, sizeof(cfg_blk_chk_enable)));
  if (isfound > 0) {
    enable_blk_num_chk_bit = rf_val & 0x40;
    if ((enable_blk_num_chk_bit != 0x40) && (cfg_blk_chk_enable == 1)) {
      phNxpNciRfSet.p_rx_data[8] |= 0x40;  // Enable if it is disabled
    } else if ((enable_blk_num_chk_bit == 0x40) && (cfg_blk_chk_enable == 0)) {
      phNxpNciRfSet.p_rx_data[8] &= ~0x40;  // Disable if it is Enabled
    } else {
      flag_send_transit_config = false;  // No need to change in RF setting
    }
  }

  if (flag_send_tianjin_config || flag_send_transit_config) {
    static uint8_t set_rf_cmd[] = {0x20, 0x02, 0x08, 0x01, 0xA0, 0x85,
                                   0x04, 0x50, 0x08, 0x68, 0x00};
    memcpy(&set_rf_cmd[4], &phNxpNciRfSet.p_rx_data[5], 7);
    status = phNxpNciHal_send_ext_cmd(sizeof(set_rf_cmd), set_rf_cmd);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("unable to set the RF setting");
      retry_cnt++;
      goto retry_send_ext;
    }
  }

  return status;
}

int check_config_parameter() {
  NFCSTATUS status = NFCSTATUS_FAILED;
  uint8_t param_clock_src = CLK_SRC_PLL;
  if (nxpprofile_ctrl.bClkSrcVal == CLK_SRC_PLL) {
      if((nfcFL.chipType != pn553) || (nfcFL.chipType != pn557)){
          param_clock_src = param_clock_src << 3;
      }

      if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_13MHZ) {
          param_clock_src |= 0x00;
      } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_19_2MHZ) {
          param_clock_src |= 0x01;
      } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_24MHZ) {
          param_clock_src |= 0x02;
      } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_26MHZ) {
          param_clock_src |= 0x03;
      } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_38_4MHZ) {
          param_clock_src |= 0x04;
      } else if (nxpprofile_ctrl.bClkFreqVal == CLK_FREQ_52MHZ) {
          param_clock_src |= 0x05;
      } else {
          NXPLOG_NCIHAL_E("Wrong clock freq, send default PLL@19.2MHz");
          param_clock_src = 0x11;
      }
  } else if (nxpprofile_ctrl.bClkSrcVal == CLK_SRC_XTAL) {
    param_clock_src = 0x08;

  } else {
    NXPLOG_NCIHAL_E("Wrong clock source. Dont apply any modification")
  }
  return param_clock_src;
}
/******************************************************************************
 * Function         phNxpNciHal_enable_i2c_fragmentation
 *
 * Description      This function is called to process the response status
 *                  and print the status byte.
 *
 * Returns          void.
 *
 ******************************************************************************/
void phNxpNciHal_enable_i2c_fragmentation() {
  NFCSTATUS status = NFCSTATUS_FAILED;
  static uint8_t fragmentation_enable_config_cmd[] = {0x20, 0x02, 0x05, 0x01,
                                                      0xA0, 0x05, 0x01, 0x10};
  int isfound = 0;
  unsigned long i2c_status = 0x00;
  unsigned long config_i2c_value = 0xff;
  /*NCI_RESET_CMD*/
  static uint8_t cmd_reset_nci[] = {0x20, 0x00, 0x01, 0x00};
  /*NCI_INIT_CMD*/
  static uint8_t cmd_init_nci[] = {0x20, 0x01, 0x00};
  static uint8_t cmd_init_nci2_0[] = {0x20,0x01,0x02,0x00,0x00};
  static uint8_t get_i2c_fragmentation_cmd[] = {0x20, 0x03, 0x03,
                                                0x01, 0xA0, 0x05};
  isfound = (GetNxpNumValue(NAME_NXP_I2C_FRAGMENTATION_ENABLED,
                            (void*)&i2c_status, sizeof(i2c_status)));
  status = phNxpNciHal_send_ext_cmd(sizeof(get_i2c_fragmentation_cmd),
                                    get_i2c_fragmentation_cmd);
  if (status != NFCSTATUS_SUCCESS) {
    NXPLOG_NCIHAL_E("unable to retrieve  get_i2c_fragmentation_cmd");
  } else {
    if (nxpncihal_ctrl.p_rx_data[8] == 0x10) {
      config_i2c_value = 0x01;
      phNxpNciHal_notify_i2c_fragmentation();
      phTmlNfc_set_fragmentation_enabled(I2C_FRAGMENTATION_ENABLED);
    } else if (nxpncihal_ctrl.p_rx_data[8] == 0x00) {
      config_i2c_value = 0x00;
    }
    if (config_i2c_value == i2c_status) {
      NXPLOG_NCIHAL_E("i2c_fragmentation_status existing");
    } else {
      if (i2c_status == 0x01) {
        /* NXP I2C fragmenation enabled*/
        status =
            phNxpNciHal_send_ext_cmd(sizeof(fragmentation_enable_config_cmd),
                                     fragmentation_enable_config_cmd);
        if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("NXP fragmentation enable failed");
        }
      } else if (i2c_status == 0x00 || config_i2c_value == 0xff) {
        fragmentation_enable_config_cmd[7] = 0x00;
        /* NXP I2C fragmentation disabled*/
        status =
            phNxpNciHal_send_ext_cmd(sizeof(fragmentation_enable_config_cmd),
                                     fragmentation_enable_config_cmd);
        if (status != NFCSTATUS_SUCCESS) {
          NXPLOG_NCIHAL_E("NXP fragmentation disable failed");
        }
      }
      status = phNxpNciHal_send_ext_cmd(sizeof(cmd_reset_nci), cmd_reset_nci);
      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("NCI_CORE_RESET: Failed");
      }

      if (nxpncihal_ctrl.nci_info.nci_version == NCI_VERSION_2_0) {
        status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci2_0), cmd_init_nci2_0);
      } else {
        status = phNxpNciHal_send_ext_cmd(sizeof(cmd_init_nci), cmd_init_nci);
      }

      if (status != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_E("NCI_CORE_INIT : Failed");
      } else if (i2c_status == 0x01) {
        phNxpNciHal_notify_i2c_fragmentation();
        phTmlNfc_set_fragmentation_enabled(I2C_FRAGMENTATION_ENABLED);
      }
    }
  }
}
/******************************************************************************
 * Function         phNxpNciHal_check_factory_reset
 *
 * Description      This function is called at init time to check
 *                  the presence of ese related info. If file are not
 *                  present set the SWP_INT_SESSION_ID_CFG to FF to
 *                  force the NFCEE to re-run its initialization sequence.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_check_factory_reset(void) {
  struct stat st;
  int ret = 0;
  NFCSTATUS status = NFCSTATUS_FAILED;
  const char config_eseinfo_path[] = "/data/vendor/nfc/nfaStorage.bin1";

  static uint8_t reset_session_identity_set[] = {
          0x20, 0x02, 0x22, 0x03, 0xA0, 0xEA, 0x08, 0xFF, 0xFF, 0xFF,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0, 0x1E, 0x08, 0xFF, 0xFF,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0, 0xEB, 0x08, 0xFF,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  static uint8_t reset_session_identity_set_stat[] = {
          0x20, 0x02, 0x17, 0x02, 0xA0, 0xEA, 0x08, 0xFF, 0xFF,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0, 0xEB, 0x08,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#ifdef PN547C2_FACTORY_RESET_DEBUG
  static uint8_t reset_session_identity[] = {0x20, 0x03, 0x05, 0x02,
                                                 0xA0, 0xEA, 0xA0, 0xEB};
#endif
  if (stat(config_eseinfo_path, &st) == -1) {
    NXPLOG_NCIHAL_D("%s file not present = %s", __func__, config_eseinfo_path);
    ret = -1;
  } else {
    ret = 0;
  }

  if (ret == -1) {
#ifdef PN547C2_FACTORY_RESET_DEBUG
    /* NXP ACT Proprietary Ext */
    status = phNxpNciHal_send_ext_cmd(sizeof(reset_session_identity),
            reset_session_identity);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NXP reset_session_identity command failed");
    }
#endif
    if((nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH) &&
            (nfcFL.nfccFL._NFCC_DYNAMIC_DUAL_UICC)) {
        status = phNxpNciHal_send_ext_cmd(sizeof(reset_session_identity_set),
                reset_session_identity_set);
    }
    else {
        status = phNxpNciHal_send_ext_cmd(sizeof(reset_session_identity_set_stat),
                reset_session_identity_set_stat);
    }
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NXP reset_session_identity_set command failed");
    }
#ifdef PN547C2_FACTORY_RESET_DEBUG
    /* NXP ACT Proprietary Ext */
    status = phNxpNciHal_send_ext_cmd(sizeof(reset_session_identity),
            reset_session_identity);
    if (status != NFCSTATUS_SUCCESS) {
      NXPLOG_NCIHAL_E("NXP reset_ese_session_identity command failed");
    }
#endif
  }
}

/******************************************************************************
 * Function         phNxpNciHal_print_res_status
 *
 * Description      This function is called to process the response status
 *                  and print the status byte.
 *
 * Returns          void.
 *
 ******************************************************************************/
static void phNxpNciHal_print_res_status(uint8_t* p_rx_data, uint16_t* p_len) {
  static uint8_t response_buf[][30] = {
      "STATUS_OK", "STATUS_REJECTED", "STATUS_RF_FRAME_CORRUPTED",
      "STATUS_FAILED", "STATUS_NOT_INITIALIZED", "STATUS_SYNTAX_ERROR",
      "STATUS_SEMANTIC_ERROR", "RFU", "RFU", "STATUS_INVALID_PARAM",
      "STATUS_MESSAGE_SIZE_EXCEEDED", "STATUS_UNDEFINED"};
  int status_byte;
  if (p_rx_data[0] == 0x40 && (p_rx_data[1] == 0x02 || p_rx_data[1] == 0x03)) {
    if (p_rx_data[2] && p_rx_data[3] <= 10) {
      status_byte = p_rx_data[CORE_RES_STATUS_BYTE];
      NXPLOG_NCIHAL_D("%s: response status =%s", __func__,
                      response_buf[status_byte]);
    } else {
      NXPLOG_NCIHAL_D("%s: response status =%s", __func__, response_buf[11]);
    }
    if (phNxpNciClock.isClockSet) {
      int i;
      for (i = 0; i < *p_len; i++) {
        phNxpNciClock.p_rx_data[i] = p_rx_data[i];
      }
    }

    if (phNxpNciRfSet.isGetRfSetting) {
      int i;
      for (i = 0; i < *p_len; i++) {
        phNxpNciRfSet.p_rx_data[i] = p_rx_data[i];
        // NXPLOG_NCIHAL_D("%s: response status =0x%x",__func__,p_rx_data[i]);
      }
    }
  }

  if ((p_rx_data[2]) && (config_access == true)) {
    if (p_rx_data[3] != NFCSTATUS_SUCCESS) {
        NXPLOG_NCIHAL_W("Invalid Data from config file.");
        config_success = false;
    }
  }
}
/******************************************************************************
 * Function         phNxpNciHal_set_Boot_Mode
 *
 * Description      This function is called to  set hal
 *                  boot mode. This can be normal nfc boot
 *                  or fast boot.The param mode can take the
 *                  following values.
 *                  NORAML_NFC_MODE = 0 FAST_BOOT_MODE = 1
 *
 *
 * Returns          void.
 *
 ******************************************************************************/
NFCSTATUS phNxpNciHal_set_Boot_Mode(uint8_t mode) {
  nxpncihal_ctrl.hal_boot_mode = mode;
  return NFCSTATUS_SUCCESS;
}

/*****************************************************************************
 * Function         phNxpNciHal_send_get_cfgs
 *
 * Description      This function is called to  send get configs
 *                  for all the types in get_cfg_arr.
 *                  Response of getConfigs(EEPROM stored) will be
 *                  compared with request coming from MW during discovery.
 *                  If same, then current setConfigs will be dropped
 *
 * Returns          Returns NFCSTATUS_SUCCESS if sending cmd is successful and
 *                  response is received.
 *
 *****************************************************************************/
NFCSTATUS phNxpNciHal_send_get_cfgs() {
  NXPLOG_NCIHAL_D("%s Enter", __func__);
  NFCSTATUS status = NFCSTATUS_FAILED;
  uint8_t num_cfgs = sizeof(get_cfg_arr) / sizeof(uint8_t);
  uint8_t cfg_count = 0, retry_cnt = 0;
  if (mGetCfg_info != NULL) {
      mGetCfg_info->isGetcfg = true;
  }
  uint8_t cmd_get_cfg[] = {0x20, 0x03, 0x02, 0x01, 0x00};

  while (cfg_count < num_cfgs) {
    cmd_get_cfg[sizeof(cmd_get_cfg) - 1] = get_cfg_arr[cfg_count];

  retry_get_cfg:
    status = phNxpNciHal_send_ext_cmd(sizeof(cmd_get_cfg), cmd_get_cfg);
    if (status != NFCSTATUS_SUCCESS && retry_cnt < 3) {
      NXPLOG_NCIHAL_E("cmd_get_cfg failed");
      retry_cnt++;
      goto retry_get_cfg;
    }
    if (retry_cnt == 3) {
      break;
    }
    cfg_count++;
    retry_cnt = 0;
  }
  mGetCfg_info->isGetcfg = false;
  return status;
}
NFCSTATUS phNxpNciHal_send_nfcee_pwr_cntl_cmd(uint8_t type) {
  NFCSTATUS status = NFCSTATUS_FAILED;
  uint8_t cmd_buf[] = {0x22, 0x03, 0x02, 0xC0, 0x00};
  uint8_t retry_cnt = 0;

  NXPLOG_NCIHAL_D("phNxpNciHal_send_nfcee_pwr_cntl_cmd; enter");
  cmd_buf[sizeof(cmd_buf) - 1] = type;
  while (status != NFCSTATUS_SUCCESS && retry_cnt < 3) {
    status = phNxpNciHal_send_ext_cmd(sizeof(cmd_buf), cmd_buf);
    NXPLOG_NCIHAL_E("cmd_nfcee_pwr_cntl_cfg failed");
    retry_cnt++;
  }
  retry_cnt = 0;
  return status;
}

/*******************************************************************************
**
** Function         phNxpNciHal_getFWDownloadFlags
**
** Description      Returns the current mode
**
** Parameters       none
**
** Returns          Current mode download/NCI
*******************************************************************************/
int phNxpNciHal_getFWDownloadFlag(uint8_t* fwDnldRequest) {
  int status = NFCSTATUS_FAILED;

  NXPLOG_NCIHAL_D("notifyFwrequest %d", notifyFwrequest);
  if (fwDnldRequest != NULL) {
    status = NFCSTATUS_OK;
    if (notifyFwrequest == true) {
      *fwDnldRequest = true;
      notifyFwrequest = false;
    } else {
      *fwDnldRequest = false;
    }
  }

  return status;
}

/*******************************************************************************
**
** Function         phNxpNciHal_configFeatureList
**
** Description      Configures the featureList based on chip type
**                  HW Version information number will provide chipType.
**                  HW Version can be obtained from CORE_INIT_RESPONSE(NCI 1.0)
**                  or CORE_RST_NTF(NCI 2.0)
**
** Parameters       CORE_INIT_RESPONSE/CORE_RST_NTF, len
**
** Returns          none
*******************************************************************************/
void phNxpNciHal_configFeatureList(uint8_t* init_rsp, uint16_t rsp_len) {
    nxpncihal_ctrl.chipType = configChipType(init_rsp,rsp_len);
    tNFC_chipType chipType = nxpncihal_ctrl.chipType;
    CONFIGURE_FEATURELIST(chipType);
    NXPLOG_NCIHAL_D("NFC_GetFeatureList ()chipType = %d", chipType);
}

/*******************************************************************************
**
** Function         phNxpNciHal_getChipType
**
** Description      Gets the chipType which is configured during bootup
**
** Parameters       none
**
** Returns          chipType
*******************************************************************************/
tNFC_chipType phNxpNciHal_getChipType() {
    return nxpncihal_ctrl.chipType;
}

