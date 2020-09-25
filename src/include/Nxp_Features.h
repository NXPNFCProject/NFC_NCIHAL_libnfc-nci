/*
 * Copyright 2012-2020 NXP
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

/*
 * NXP features declarations
 */
#include <unistd.h>
#include <string.h>
#ifndef NXP_FEATURES_H
#define NXP_FEATURES_H

#define STRMAX_1 40
#define STRMAX_2 100

#ifndef FW_DLL_ROOT_DIR
#ifdef __LP64__
#define FW_DLL_ROOT_DIR "/system/vendor/lib64/"
#else
#define FW_DLL_ROOT_DIR "/system/vendor/lib/"
#endif
#endif

#define FW_DLL_EXTENSION ".so"

#define FW_MOBILE_MAJOR_NUMBER_PN553 0x01
#define FW_MOBILE_MAJOR_NUMBER_PN551 0x05
#define FW_MOBILE_MAJOR_NUMBER_PN48AD 0x01

#define NFA_EE_MAX_EE_SUPPORTED 5

#define JCOP_VER_3_1    1
#define JCOP_VER_3_2    2
#define JCOP_VER_3_3    3
#define JCOP_VER_4_0    4

typedef enum {
    unknown,
    pn547C2,
    pn65T,
    pn548C2,
    pn66T,
    pn551,
    pn67T,
    pn553,
    pn80T,
    pn557,
    pn81T
}tNFC_chipType;

/*
 * TODO renaming
 * */
typedef struct {
    /*Flags common to all chip types*/
    uint8_t _NXP_NFCC_EMPTY_DATA_PACKET                     : 1;
    uint8_t _GEMALTO_SE_SUPPORT                             : 1;
    uint8_t _NFCC_I2C_READ_WRITE_IMPROVEMENT                : 1;
    uint8_t _NFCC_MIFARE_TIANJIN                            : 1;
    uint8_t _NFCC_MW_RCVRY_BLK_FW_DNLD                      : 1;
    uint8_t _NFCC_DYNAMIC_DUAL_UICC                         : 1;
    uint8_t _NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH              : 1;
    uint8_t _NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH           : 1;
    uint8_t _NFCC_FW_WA                                     : 1;
    uint8_t _NFCC_FORCE_NCI1_0_INIT                         : 1;
    uint8_t _NFCC_ROUTING_BLOCK_BIT                         : 1;
    uint8_t _NFCC_SPI_FW_DOWNLOAD_SYNC                      : 1;
    uint8_t _HW_ANTENNA_LOOP4_SELF_TEST                     : 1;
    uint8_t _NFCEE_REMOVED_NTF_RECOVERY                     : 1;
    uint8_t _NFCC_FORCE_FW_DOWNLOAD                         : 1;
    uint8_t _UICC_CREATE_CONNECTIVITY_PIPE                  : 1;
    uint8_t _NFCC_AID_MATCHING_PLATFORM_CONFIG              : 1;
    uint8_t _NFCC_ROUTING_BLOCK_BIT_PROP                    : 1;
    uint8_t _NXP_NFC_UICC_ETSI12                            : 1;
    uint8_t _NFA_EE_MAX_EE_SUPPORTED                        : 3;
}tNfc_nfccFeatureList;
/*
 * TODO renaming
 * */
typedef struct {
    uint8_t _ESE_EXCLUSIVE_WIRED_MODE                    : 2;
    uint8_t _ESE_WIRED_MODE_RESUME                       : 2;
    uint8_t _ESE_WIRED_MODE_TIMEOUT                      : 2;
    uint8_t _ESE_PN67T_RESET                             : 2;
    uint8_t _ESE_APDU_GATE_RESET                         : 2;
    uint8_t _ESE_WIRED_MODE_DISABLE_DISCOVERY            : 1;
    uint8_t _LEGACY_APDU_GATE                            : 1;
    uint8_t _TRIPLE_MODE_PROTECTION                      : 1;
    uint8_t _ESE_FELICA_CLT                              : 1;
    uint8_t _WIRED_MODE_STANDBY_PROP                     : 1;
    uint8_t _WIRED_MODE_STANDBY                          : 1;
    uint8_t _ESE_DUAL_MODE_PRIO_SCHEME                   : 2;
    uint8_t _ESE_FORCE_ENABLE                            : 1;
    uint8_t _ESE_RESET_METHOD                            : 1;
    uint8_t _EXCLUDE_NV_MEM_DEPENDENCY                   : 1;
    uint8_t _ESE_ETSI_READER_ENABLE                      : 1;
    uint8_t _ESE_SVDD_SYNC                               : 1;
    uint8_t _NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION  : 1;
    uint8_t _ESE_JCOP_DWNLD_PROTECTION                   : 1;
    uint8_t _UICC_HANDLE_CLEAR_ALL_PIPES                 : 1;
    uint8_t _GP_CONTINOUS_PROCESSING                     : 1;
    uint8_t _ESE_DWP_SPI_SYNC_ENABLE                     : 1;
    uint8_t _ESE_ETSI12_PROP_INIT                        : 1;
    uint8_t _ESE_WIRED_MODE_PRIO                         : 1;
    uint8_t _ESE_UICC_EXCLUSIVE_WIRED_MODE               : 1;
    uint8_t _ESE_POWER_MODE                              : 1;
    uint8_t _ESE_P73_ISO_RST                             : 1;
    uint8_t _BLOCK_PROPRIETARY_APDU_GATE                 : 1;
    uint8_t _JCOP_WA_ENABLE                              : 1;
    uint8_t _NXP_LDR_SVC_VER_2                           : 1;
    uint8_t _NXP_SPI_DWP_SYNC                            : 1;
    uint8_t _NXP_ESE_VER                                 : 3;
}tNfc_eseFeatureList;
/*
 * TODO renaming
 * */
typedef struct {
    uint8_t _NFCC_RESET_RSP_LEN;
}tNfc_platformFeatureList;

typedef struct {
    uint8_t _NCI_INTERFACE_UICC_DIRECT;
    uint8_t _NCI_INTERFACE_ESE_DIRECT;
    uint8_t _NCI_PWR_LINK_PARAM_CMD_SIZE;
    uint8_t _NCI_EE_PWR_LINK_ALWAYS_ON;
    uint8_t _NFA_EE_MAX_AID_ENTRIES;
    uint8_t _NFC_NXP_AID_MAX_SIZE_DYN : 1;
    char    _FW_LIB_PATH[STRMAX_2];
    char    _PLATFORM_LIB_PATH[STRMAX_2];
    char    _PKU_LIB_PATH[STRMAX_2];
    uint16_t _PHDNLDNFC_USERDATA_EEPROM_OFFSET;
    uint16_t _PHDNLDNFC_USERDATA_EEPROM_LEN;
    uint8_t  _FW_MOBILE_MAJOR_NUMBER;
}tNfc_nfcMwFeatureList;
/*
 * TODO renaming
 * */
typedef struct {
    uint8_t nfcNxpEse : 1;
    tNFC_chipType chipType;
    tNfc_nfccFeatureList nfccFL;
    tNfc_eseFeatureList eseFL;
    tNfc_platformFeatureList platformFL;
    tNfc_nfcMwFeatureList nfcMwFL;
}tNfc_featureList;

extern tNfc_featureList nfcFL;

#define CONFIGURE_FEATURELIST(chipType) {                                   \
        nfcFL.chipType = chipType;                                          \
        nfcFL.nfcMwFL._NFC_NXP_AID_MAX_SIZE_DYN = true;                     \
        if(chipType == pn81T) {                                             \
            nfcFL.chipType = pn557;                                         \
        }                                                                   \
        else if(chipType == pn80T) {                                        \
            nfcFL.chipType = pn553;                                         \
        }                                                                   \
        else if(chipType == pn67T) {                                        \
            nfcFL.chipType = pn551;                                         \
        }                                                                   \
        else if(chipType == pn66T) {                                        \
            nfcFL.chipType = pn548C2;                                       \
        }                                                                   \
        else if(chipType == pn65T) {                                        \
            nfcFL.chipType = pn547C2;                                       \
        }                                                                   \
        if ((chipType == pn65T) || (chipType == pn66T) ||                   \
                (chipType == pn67T) || (chipType == pn80T) ||               \
                (chipType == pn81T)) {                                      \
            nfcFL.nfcNxpEse = true;                                         \
            CONFIGURE_FEATURELIST_NFCC_WITH_ESE(chipType)                   \
        } \
        else {                                                              \
            nfcFL.nfcNxpEse = false;                                        \
            CONFIGURE_FEATURELIST_NFCC(chipType)                            \
        }                                                                   \
        \
        \
}

#define CONFIGURE_FEATURELIST_NFCC_WITH_ESE(chipType) {                     \
        nfcFL.nfccFL._NXP_NFCC_EMPTY_DATA_PACKET = true;                    \
        nfcFL.nfccFL._GEMALTO_SE_SUPPORT = true;                            \
        \
        \
        nfcFL.eseFL._ESE_EXCLUSIVE_WIRED_MODE = 1;                          \
        nfcFL.eseFL._ESE_WIRED_MODE_RESUME = 2;                             \
        nfcFL.eseFL._ESE_PN67T_RESET = 1;                                   \
        nfcFL.eseFL._ESE_APDU_GATE_RESET = 2;                               \
        nfcFL.eseFL._NXP_ESE_VER = JCOP_VER_4_0;                            \
        nfcFL.eseFL._NXP_LDR_SVC_VER_2 = true;                              \
        \
        \
        if (chipType == pn81T) {                                            \
            CONFIGURE_FEATURELIST_NFCC(pn557)                               \
            nfcFL.nfccFL._NFCC_SPI_FW_DOWNLOAD_SYNC = true;                 \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 4;                      \
            \
            \
            nfcFL.eseFL._ESE_FELICA_CLT = true;                             \
            nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME =                        \
            nfcFL.eseFL._ESE_WIRED_MODE_RESUME;                             \
            nfcFL.eseFL._ESE_RESET_METHOD = true;                           \
            nfcFL.eseFL._ESE_POWER_MODE = true;                             \
            nfcFL.eseFL._ESE_P73_ISO_RST = true;                            \
            nfcFL.eseFL._WIRED_MODE_STANDBY = true;                         \
            nfcFL.eseFL._ESE_ETSI_READER_ENABLE = true;                     \
            nfcFL.eseFL._ESE_SVDD_SYNC = true;                              \
            nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION = true;                  \
            nfcFL.eseFL._UICC_HANDLE_CLEAR_ALL_PIPES = true;                \
            nfcFL.eseFL._GP_CONTINOUS_PROCESSING = false;                   \
            nfcFL.eseFL._ESE_DWP_SPI_SYNC_ENABLE = true;                    \
            nfcFL.eseFL._ESE_ETSI12_PROP_INIT = true;                       \
            nfcFL.eseFL._NXP_SPI_DWP_SYNC = true;                           \
        }                                                                   \
        if (chipType == pn80T) {                                            \
            CONFIGURE_FEATURELIST_NFCC(pn553)                               \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 4;                      \
            \
            \
            nfcFL.eseFL._ESE_FELICA_CLT = true;                             \
            nfcFL.eseFL._WIRED_MODE_STANDBY = true;                         \
            nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME =                        \
            nfcFL.eseFL._ESE_WIRED_MODE_RESUME;                             \
            nfcFL.eseFL._ESE_RESET_METHOD = true;                           \
            nfcFL.eseFL._ESE_ETSI_READER_ENABLE = true;                     \
            nfcFL.eseFL._ESE_SVDD_SYNC = true;                              \
            nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION = true;                  \
            nfcFL.eseFL._UICC_HANDLE_CLEAR_ALL_PIPES = true;                \
            nfcFL.eseFL._ESE_DWP_SPI_SYNC_ENABLE = true;                    \
            nfcFL.eseFL._ESE_POWER_MODE = true;                             \
            nfcFL.eseFL._ESE_P73_ISO_RST = true;                            \
            nfcFL.eseFL._NXP_SPI_DWP_SYNC = false;                          \
            \
            \
            nfcFL.nfcMwFL._NCI_PWR_LINK_PARAM_CMD_SIZE = 0x02;              \
            nfcFL.nfcMwFL._NCI_EE_PWR_LINK_ALWAYS_ON = 0x01;                \
        }                                                                   \
        else if (chipType == pn67T)                                         \
        {                                                                   \
            CONFIGURE_FEATURELIST_NFCC(pn551)                               \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 3;                      \
            \
            \
            nfcFL.eseFL._TRIPLE_MODE_PROTECTION = true;                     \
            nfcFL.eseFL._WIRED_MODE_STANDBY_PROP = true;                    \
            nfcFL.eseFL._ESE_FORCE_ENABLE = true;                           \
            nfcFL.eseFL._ESE_ETSI_READER_ENABLE = true;                     \
            nfcFL.eseFL._ESE_SVDD_SYNC = true;                              \
            nfcFL.eseFL._LEGACY_APDU_GATE = true;                           \
            nfcFL.eseFL._NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION = true; \
            nfcFL.eseFL._ESE_DWP_SPI_SYNC_ENABLE = true;                    \
            nfcFL.eseFL._NXP_ESE_VER = JCOP_VER_3_3;                        \
            nfcFL.eseFL._NXP_SPI_DWP_SYNC = false;                          \
        }                                                                   \
        else if (chipType == pn66T)                                         \
        {                                                                   \
            CONFIGURE_FEATURELIST_NFCC(pn548C2)                             \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 3;                      \
            \
            \
            nfcFL.eseFL._TRIPLE_MODE_PROTECTION = true;                     \
            nfcFL.eseFL._WIRED_MODE_STANDBY_PROP = true;                    \
            nfcFL.eseFL._ESE_FORCE_ENABLE = true;                           \
            nfcFL.eseFL._ESE_ETSI_READER_ENABLE = true;                     \
            nfcFL.eseFL._ESE_SVDD_SYNC = true;                              \
            nfcFL.eseFL._LEGACY_APDU_GATE = true;                           \
            nfcFL.eseFL._NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION = true; \
            nfcFL.eseFL._ESE_DWP_SPI_SYNC_ENABLE = true;                    \
            nfcFL.eseFL._NXP_ESE_VER = JCOP_VER_3_3;                        \
            nfcFL.eseFL._NXP_SPI_DWP_SYNC = false;                          \
        }                                                                   \
        else if (chipType == pn65T)                                         \
        {                                                                   \
            CONFIGURE_FEATURELIST_NFCC(pn547C2)                             \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 3;                      \
            nfcFL.eseFL._ESE_WIRED_MODE_DISABLE_DISCOVERY = true;           \
            nfcFL.eseFL._LEGACY_APDU_GATE = true;                           \
        }                                                                   \
}


#define CONFIGURE_FEATURELIST_NFCC(chipType) {                              \
        nfcFL.eseFL._ESE_WIRED_MODE_TIMEOUT = 3;                            \
        nfcFL.eseFL._ESE_WIRED_MODE_DISABLE_DISCOVERY = false;              \
        nfcFL.eseFL._LEGACY_APDU_GATE = false;                              \
        nfcFL.eseFL._TRIPLE_MODE_PROTECTION = false;                        \
        nfcFL.eseFL._ESE_FELICA_CLT = false;                                \
        nfcFL.eseFL._WIRED_MODE_STANDBY_PROP = false;                       \
        nfcFL.eseFL._WIRED_MODE_STANDBY = false;                            \
        nfcFL.eseFL._ESE_DUAL_MODE_PRIO_SCHEME =                            \
        nfcFL.eseFL._ESE_WIRED_MODE_TIMEOUT;                                \
        nfcFL.eseFL._ESE_FORCE_ENABLE = false;                              \
        nfcFL.eseFL._ESE_RESET_METHOD = false;                              \
        nfcFL.eseFL._ESE_ETSI_READER_ENABLE = false;                        \
        nfcFL.eseFL._ESE_SVDD_SYNC = false;                                 \
        nfcFL.eseFL._NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION = false;    \
        nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION = false;                     \
        nfcFL.eseFL._UICC_HANDLE_CLEAR_ALL_PIPES = false;                   \
        nfcFL.eseFL._GP_CONTINOUS_PROCESSING = false;                       \
        nfcFL.eseFL._ESE_DWP_SPI_SYNC_ENABLE = false;                       \
        nfcFL.eseFL._ESE_ETSI12_PROP_INIT = false;                          \
        nfcFL.eseFL._ESE_WIRED_MODE_PRIO = false;                           \
        nfcFL.eseFL._ESE_UICC_EXCLUSIVE_WIRED_MODE = false;                 \
        nfcFL.eseFL._ESE_POWER_MODE = false;                                \
        nfcFL.eseFL._ESE_P73_ISO_RST = false;                               \
        nfcFL.eseFL._BLOCK_PROPRIETARY_APDU_GATE = false;                   \
        nfcFL.eseFL._JCOP_WA_ENABLE = true;                                 \
        nfcFL.eseFL._EXCLUDE_NV_MEM_DEPENDENCY = false;                     \
        nfcFL.eseFL._NXP_SPI_DWP_SYNC = false;                              \
        nfcFL.nfccFL._NXP_NFC_UICC_ETSI12 = false;                          \
        nfcFL.nfccFL._NFCC_SPI_FW_DOWNLOAD_SYNC = false;                    \
        \
        \
        nfcFL.platformFL._NFCC_RESET_RSP_LEN = 0;                           \
        \
        \
        nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT = 0x00;                    \
        nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT = 0x00;                     \
        nfcFL.nfcMwFL._NCI_PWR_LINK_PARAM_CMD_SIZE = 0x02;                  \
        nfcFL.nfcMwFL._NCI_EE_PWR_LINK_ALWAYS_ON = 0x01;                    \
        nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_OFFSET = 0x023CU;          \
        nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_LEN = 0x0C80U;             \
        nfcFL.nfcMwFL._FW_MOBILE_MAJOR_NUMBER =                             \
        FW_MOBILE_MAJOR_NUMBER_PN48AD;                                      \
        \
        \
        if (chipType == pn557)                                              \
        {                                                                   \
            nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT = true;           \
            nfcFL.nfccFL._NFCC_MIFARE_TIANJIN = false;                      \
            nfcFL.nfccFL._NFCC_MW_RCVRY_BLK_FW_DNLD = true;                 \
            nfcFL.nfccFL._NFCC_DYNAMIC_DUAL_UICC = true;                    \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH = false;        \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH = true;      \
            nfcFL.nfccFL._NFCC_FW_WA = true;                                \
            nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT = false;                   \
            nfcFL.nfccFL._NFCC_SPI_FW_DOWNLOAD_SYNC = true;                 \
            nfcFL.nfccFL._HW_ANTENNA_LOOP4_SELF_TEST = false;               \
            nfcFL.nfccFL._NFCEE_REMOVED_NTF_RECOVERY = true;                \
            nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD = true;                    \
            nfcFL.nfccFL._UICC_CREATE_CONNECTIVITY_PIPE = true;             \
            nfcFL.nfccFL._NXP_NFC_UICC_ETSI12 = false;                      \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 3;                      \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT = true;                    \
            \
            \
            nfcFL.eseFL._ESE_ETSI12_PROP_INIT = true;                       \
            nfcFL.eseFL._EXCLUDE_NV_MEM_DEPENDENCY = true;                  \
            \
            \
            nfcFL.platformFL._NFCC_RESET_RSP_LEN = 0x10U;                   \
            \
            \
            nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT = 0x82;                \
            nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT = 0x83;                 \
            \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_OFFSET = 0x02BCU;      \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_LEN = 0x0BC0U;         \
            \
        }                                                                   \
        else if (chipType == pn553)                                         \
        {                                                                   \
            nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT = true;           \
            nfcFL.nfccFL._NFCC_MIFARE_TIANJIN = false;                      \
            nfcFL.nfccFL._NFCC_MW_RCVRY_BLK_FW_DNLD = true;                 \
            nfcFL.nfccFL._NFCC_DYNAMIC_DUAL_UICC = true;                    \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH = false;        \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH = true;      \
            nfcFL.nfccFL._NFCC_FW_WA = true;                                \
            nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT = true;                    \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT = true;                    \
            nfcFL.nfccFL._NFCC_SPI_FW_DOWNLOAD_SYNC = true;                 \
            nfcFL.nfccFL._HW_ANTENNA_LOOP4_SELF_TEST = false;               \
            nfcFL.nfccFL._NFCEE_REMOVED_NTF_RECOVERY = true;                \
            nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD = true;                    \
            nfcFL.nfccFL._UICC_CREATE_CONNECTIVITY_PIPE = true;             \
            nfcFL.nfccFL._NFCC_AID_MATCHING_PLATFORM_CONFIG = false;        \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT_PROP = false;              \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 3;                      \
            \
            \
            nfcFL.eseFL._ESE_ETSI12_PROP_INIT = true;                       \
            nfcFL.eseFL._JCOP_WA_ENABLE = false;                            \
            nfcFL.eseFL._EXCLUDE_NV_MEM_DEPENDENCY = true;                  \
            \
            \
            nfcFL.platformFL._NFCC_RESET_RSP_LEN = 0x10U;                   \
            \
            \
            nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT = 0x82;                \
            nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT = 0x83;                 \
            \
            \
            SRTCPY_FW("libpn553tc_fw", "libpn553tc_fw_platform",            \
                    "libpn553tc_fw_pku")                                    \
            \
            \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_OFFSET = 0x02BCU;      \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_LEN = 0x0BC0U;         \
            nfcFL.nfcMwFL._FW_MOBILE_MAJOR_NUMBER =                         \
            FW_MOBILE_MAJOR_NUMBER_PN553;                                   \
            \
            \
        }                                                                   \
        else if (chipType == pn551)                                         \
        {                                                                   \
            nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT = true;           \
            nfcFL.nfccFL._NFCC_MIFARE_TIANJIN = true;                       \
            nfcFL.nfccFL._NFCC_MW_RCVRY_BLK_FW_DNLD = false;                \
            nfcFL.nfccFL._NFCC_DYNAMIC_DUAL_UICC = false;                   \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH = true;         \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH = false;     \
            nfcFL.nfccFL._NFCC_FW_WA = false;                               \
            nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT = false;                   \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT = false;                   \
            nfcFL.nfccFL._NFCC_SPI_FW_DOWNLOAD_SYNC = false;                \
            nfcFL.nfccFL._HW_ANTENNA_LOOP4_SELF_TEST = true;                \
            nfcFL.nfccFL._NFCEE_REMOVED_NTF_RECOVERY = true;                \
            nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD = false;                   \
            nfcFL.nfccFL._UICC_CREATE_CONNECTIVITY_PIPE = false;            \
            nfcFL.nfccFL._NFCC_AID_MATCHING_PLATFORM_CONFIG = true;         \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT_PROP = true;               \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 2;                      \
            \
            \
            nfcFL.eseFL._ESE_FORCE_ENABLE = true;                           \
            \
            \
            nfcFL.platformFL._NFCC_RESET_RSP_LEN = 0x11U;                   \
            \
            \
            nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT = 0x82;                \
            nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT = 0x83;                 \
            \
            \
            SRTCPY_FW("libpn551_fw", "libpn551_fw_platform",                \
                    "libpn551_fw_pku")                                      \
            \
            \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_OFFSET = 0x02BCU;      \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_LEN = 0x0C00U;         \
            nfcFL.nfcMwFL._FW_MOBILE_MAJOR_NUMBER =                         \
            FW_MOBILE_MAJOR_NUMBER_PN551;                                   \
            \
            \
        }                                                                   \
        else if (chipType == pn548C2)                                       \
        {                                                                   \
            nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT = true;           \
            nfcFL.nfccFL._NFCC_MIFARE_TIANJIN = true;                       \
            nfcFL.nfccFL._NFCC_MW_RCVRY_BLK_FW_DNLD = false;                \
            nfcFL.nfccFL._NFCC_DYNAMIC_DUAL_UICC = false;                   \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH = true;         \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH = false;     \
            nfcFL.nfccFL._NFCC_FW_WA = false;                               \
            nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT = false;                   \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT = false;                   \
            nfcFL.nfccFL._NFCC_SPI_FW_DOWNLOAD_SYNC = false;                \
            nfcFL.nfccFL._HW_ANTENNA_LOOP4_SELF_TEST = true;                \
            nfcFL.nfccFL._NFCEE_REMOVED_NTF_RECOVERY = true;                \
            nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD = false;                   \
            nfcFL.nfccFL._UICC_CREATE_CONNECTIVITY_PIPE = false;            \
            nfcFL.nfccFL._NFCC_AID_MATCHING_PLATFORM_CONFIG = true;         \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT_PROP = true;               \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 2;                      \
            \
            \
            nfcFL.eseFL._ESE_FORCE_ENABLE = true;                           \
            nfcFL.platformFL._NFCC_RESET_RSP_LEN = 0x11U;                   \
            \
            \
            nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT = 0x82;                \
            nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT = 0x83;                 \
            \
            \
            SRTCPY_FW("libpn548ad_fw", "libpn548ad_fw_platform",            \
                    "libpn548ad_fw_pku")                                    \
            \
            \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_OFFSET = 0x02BCU;      \
            nfcFL.nfcMwFL._PHDNLDNFC_USERDATA_EEPROM_LEN = 0x0C00U;         \
            \
            \
        }                                                                   \
        else if(chipType == pn547C2)                                        \
        {                                                                   \
            nfcFL.nfccFL._NFCC_I2C_READ_WRITE_IMPROVEMENT = false;          \
            nfcFL.nfccFL._NFCC_MIFARE_TIANJIN = true;                       \
            nfcFL.nfccFL._NFCC_MW_RCVRY_BLK_FW_DNLD = false;                \
            nfcFL.nfccFL._NFCC_DYNAMIC_DUAL_UICC = false;                   \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_EXT_SWITCH = false;        \
            nfcFL.nfccFL._NFC_NXP_STAT_DUAL_UICC_WO_EXT_SWITCH = false;     \
            nfcFL.nfccFL._NFCC_FW_WA = false;                               \
            nfcFL.nfccFL._NFCC_FORCE_NCI1_0_INIT = false;                   \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT = false;                   \
            nfcFL.nfccFL._NFCC_SPI_FW_DOWNLOAD_SYNC = false;                \
            nfcFL.nfccFL._HW_ANTENNA_LOOP4_SELF_TEST = true;                \
            nfcFL.nfccFL._NFCEE_REMOVED_NTF_RECOVERY = true;                \
            nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD = false;                   \
            nfcFL.nfccFL._UICC_CREATE_CONNECTIVITY_PIPE = false;            \
            nfcFL.nfccFL._NFCC_AID_MATCHING_PLATFORM_CONFIG = true;         \
            nfcFL.nfccFL._NFCC_ROUTING_BLOCK_BIT_PROP = false;              \
            nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 2;                      \
            \
            \
            nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT = 0x81;                \
            nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT = 0x82;                 \
            nfcFL.platformFL._NFCC_RESET_RSP_LEN = 0x11U;                   \
            \
            \
            SRTCPY_FW("libpn547_fw", "libpn547_fw_platform",                \
                    "libpn547_fw_pku")                                      \
            \
            \
        }                                                                   \
}
#ifdef __cplusplus
#define SRTCPY_FW(str1,str2,str3)
#else
#define SRTCPY_FW(str1, str2,str3)                                                      \
        snprintf(nfcFL.nfcMwFL._FW_LIB_PATH, STRMAX_2, "%s%s%s",                        \
                FW_DLL_ROOT_DIR, str1, FW_DLL_EXTENSION);                               \
                snprintf(nfcFL.nfcMwFL._PLATFORM_LIB_PATH, STRMAX_2, "%s%s%s",          \
                        FW_DLL_ROOT_DIR, str2, FW_DLL_EXTENSION);                       \
                        snprintf(nfcFL.nfcMwFL._PKU_LIB_PATH, STRMAX_2, "%s%s%s",       \
                                FW_DLL_ROOT_DIR, str3, FW_DLL_EXTENSION);
#endif
#endif
