/******************************************************************************
 *
 *  Copyright 2018-2019 NXP
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
 ******************************************************************************/
#ifndef ANDROID_HARDWARE_HAL_NXPNFC_V1_0_H
#define ANDROID_HARDWARE_HAL_NXPNFC_V1_0_H

#define NFC_NCI_NXP_PN54X_HARDWARE_MODULE_ID "nfc_nci.pn54x"
#define MAX_IOCTL_TRANSCEIVE_CMD_LEN  256
#define MAX_IOCTL_TRANSCEIVE_RESP_LEN 256
#define MAX_ATR_INFO_LEN              128
#define NCI_ESE_HARD_RESET_IOCTL      5
#define HAL_NFC_IOCTL_FIRST_EVT 0xA0
enum {
    HAL_NFC_IOCTL_NCI_TRANSCEIVE = 0xF1,
    HAL_NFC_IOCTL_ESE_HARD_RESET,
};

enum {
  HAL_NFC_IOCTL_CHECK_FLASH_REQ = HAL_NFC_IOCTL_FIRST_EVT,
  HAL_NFC_IOCTL_GET_FEATURE_LIST,
  HAL_NFC_SET_SPM_PWR,
  HAL_NFC_IOCTL_ESE_JCOP_DWNLD,
  HAL_NFC_IOCTL_ESE_UPDATE_COMPLETE
#if (NXP_EXTNS == TRUE)
 ,HAL_NFC_IOCTL_SET_TRANSIT_CONFIG,
  HAL_NFC_IOCTL_GET_ESE_UPDATE_STATE,
  HAL_NFC_IOCTL_GET_NXP_CONFIG,
#endif
};
enum {
    //HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT = 0x07,
    HAL_NFC_POST_MIN_INIT_CPLT_EVT       = 0x08
};
/*
 * Data structures provided below are used of Hal Ioctl calls
 */
/*
 * nfc_nci_ExtnCmd_t shall contain data for commands used for transceive command in ioctl
 */
typedef struct
{
    uint16_t cmd_len;
    uint8_t  p_cmd[MAX_IOCTL_TRANSCEIVE_CMD_LEN];
} nfc_nci_ExtnCmd_t;

#if(NXP_EXTNS == TRUE)
/*
 * nxp_nfc_rfStorage_t shall contain rf config file storage path and
 * length of the path
 */
typedef struct {
  long len;
  char path[264];
}nxp_nfc_rfStorage_t;
/*
 * nxp_nfc_fwStorage_t shall contain fw config file storage path and
 * length of the path
 */
typedef struct {
  long len;
  char path[264];
}nxp_nfc_fwStorage_t;
/*
 * nxp_nfc_coreConf_t shall contain core conf command and
 * length of the command
 */
typedef struct {
  long len;
  uint8_t cmd[264];
}nxp_nfc_coreConf_t;
/*
 * nxp_nfc_rfFileVerInfo_t shall contain rf file version info and
 *length of it
 */
typedef struct {
  long len;
  uint8_t ver[2];
}nxp_nfc_rfFileVerInfo_t;
/*
 * nxp_nfc_config_t shall contain the respective flag value from the
 * libnfc-nxp.conf
 */
typedef struct {
  uint8_t eSeLowTempErrorDelay;
  uint8_t tagOpTimeout;
  uint8_t dualUiccEnable;
  uint8_t defaultAidRoute;
  uint8_t defaultMifareCltRoute;
  uint8_t defautlFelicaCltRoute;
  uint8_t defautlIsoDepRoute;
  uint8_t defaultAidPwrState;
  uint8_t defaultDesfirePwrState;
  uint8_t defaultMifareCltPwrState;
  uint8_t hostListenTechMask;
  uint8_t fwdFunctionalityEnable;
  uint8_t gsmaPwrState;
  uint8_t offHostRoute;
  uint8_t defaultUicc2Select;
  uint8_t smbTransceiveTimeout;
  uint8_t smbErrorRetry;
  uint8_t felicaCltPowerState;
  uint8_t checkDefaultProtoSeId;
  uint8_t nxpLogHalLoglevel;
  uint8_t nxpLogExtnsLogLevel;
  uint8_t nxpLogTmlLogLevel;
  uint8_t nxpLogFwDnldLogLevel;
  uint8_t nxpLogNcixLogLevel;
  uint8_t nxpLogNcirLogLevel;
  uint8_t seApduGateEnabled;
  uint8_t pollEfdDelay;
  uint8_t mergeSakEnable;
  uint8_t stagTimeoutCfg;
  nxp_nfc_rfStorage_t rfStorage;
  nxp_nfc_fwStorage_t fwStorage;
  nxp_nfc_coreConf_t coreConf;
  nxp_nfc_rfFileVerInfo_t rfFileVersInfo;
} nxp_nfc_config_t;
#endif
/*
 * nfc_nci_ExtnRsp_t shall contain response for command sent in transceive command
 */
typedef struct
{
    uint16_t rsp_len;
    uint8_t  p_rsp[MAX_IOCTL_TRANSCEIVE_RESP_LEN];
} nfc_nci_ExtnRsp_t;
#if(NXP_EXTNS == TRUE)
/*
 * NxpConfig_t shall contain Nxp config value and
 * Configuration length
 */
typedef struct {
  long len;
  char *val;
} NxpConfig_t;
#endif
/*
 * TransitConfig_t shall contain transit config value and transit
 * Configuration length
 */
typedef struct {
  long len;
  char *val;
} TransitConfig_t;
/*
 * InputData_t :ioctl has multiple subcommands
 * Each command has corresponding input data which needs to be populated in this
 */
typedef union {
    uint16_t          bootMode;
    uint8_t           halType;
    nfc_nci_ExtnCmd_t nciCmd;
    uint32_t          timeoutMilliSec;
    long              nfcServicePid;
    TransitConfig_t transitConfig;
#if(NXP_EXTNS == TRUE)
    NxpConfig_t nxpConfig;
#endif
}InputData_t;
/*
 * nfc_nci_ExtnInputData_t :Apart from InputData_t, there are context data
 * which is required during callback from stub to proxy.
 * To avoid additional copy of data while propagating from libnfc to Adaptation
 * and Nfcstub to ncihal, common structure is used. As a sideeffect, context data
 * is exposed to libnfc (Not encapsulated).
 */
typedef struct {
    /*context to be used/updated only by users of proxy & stub of Nfc.hal
    * i.e, NfcAdaptation & hardware/interface/Nfc.
    */
    void*       context;
    InputData_t data;
    uint8_t data_source;
    long level;
}nfc_nci_ExtnInputData_t;

/*
 * outputData_t :ioctl has multiple commands/responses
 * This contains the output types for each ioctl.
 */
typedef union{
    uint32_t            status;
    nfc_nci_ExtnRsp_t   nciRsp;
    uint8_t             nxpNciAtrInfo[MAX_ATR_INFO_LEN];
    uint32_t            p61CurrentState;
    uint16_t            fwUpdateInf;
    uint16_t            fwDwnldStatus;
    uint16_t            fwMwVerStatus;
    uint8_t             chipType;
#if(NXP_EXTNS == TRUE)
    nxp_nfc_config_t nxpConfigs;
#endif
}outputData_t;

/*
 * nfc_nci_ExtnOutputData_t :Apart from outputData_t, there are other information
 * which is required during callback from stub to proxy.
 * For ex (context, result of the operation , type of ioctl which was completed).
 * To avoid additional copy of data while propagating from libnfc to Adaptation
 * and Nfcstub to ncihal, common structure is used. As a sideeffect, these data
 * is exposed(Not encapsulated).
 */
typedef struct {
    /*ioctlType, result & context to be used/updated only by users of
     * proxy & stub of Nfc.hal.
     * i.e, NfcAdaptation & hardware/interface/Nfc
     * These fields shall not be used by libnfc or halimplementation*/
    uint64_t     ioctlType;
    uint32_t     result;
    void*        context;
    outputData_t data;
}nfc_nci_ExtnOutputData_t;

/*
 * nfc_nci_IoctlInOutData_t :data structure for input & output
 * to be sent for ioctl command. input is populated by client/proxy side
 * output is provided from server/stub to client/proxy
 */
typedef struct {
    nfc_nci_ExtnInputData_t  inp;
    nfc_nci_ExtnOutputData_t out;
}nfc_nci_IoctlInOutData_t;

enum NxpNfcHalStatus {
    /** In case of an error, HCI network needs to be re-initialized */
    HAL_NFC_STATUS_RESTART = 0x30,
    HAL_NFC_HCI_NV_RESET = 0x40,
};
typedef union {
    uint8_t ese_jcop_download_state;
} nfcIoctlData_t;
extern nfcIoctlData_t  nfcioctldata;

/*
 * nxpnfc_nci_device_t :data structure for nxp's extended nfc_nci_device
 * Extra features added are
 * -ioctl(manage sync between  and DWP & SPI)
 * -check request for fw download
 */
typedef struct nxpnfc_nci_device{
    //nfc_nci_device_t nci_device;
    /*
    * (*ioctl)() For P61 power management synchronization
    * between NFC Wired and SPI.
    */
    int (*ioctl)(const struct nxpnfc_nci_device *p_dev, long arg, void *p_data);
    /*
    * (*check_fw_dwnld_flag)() Is called to get FW downlaod request.
    */
    int (*check_fw_dwnld_flag)(const struct nxpnfc_nci_device *p_dev, uint8_t* param1);
}nxpnfc_nci_device_t;

#endif  // ANDROID_HARDWARE_HAL_NXPNFC_V1_0_H
