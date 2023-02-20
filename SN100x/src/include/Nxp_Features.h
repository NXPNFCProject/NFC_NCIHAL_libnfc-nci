/******************************************************************************
 *
 *  Copyright 2018-2023 NXP
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

#if (NXP_EXTNS == TRUE)
#include <stdint.h>
#else
#include <unistd.h>
#endif
#include <string>
#ifndef NXP_FEATURES_H
#define NXP_FEATURES_H

/*Including T4T NFCEE by incrementing 1*/
#define NFA_EE_MAX_EE_SUPPORTED 6

using namespace std;
typedef enum {
    NFCC_DWNLD_WITH_VEN_RESET,
    NFCC_DWNLD_WITH_NCI_CMD
} tNFCC_DnldType;

typedef enum {
  DEFAULT_CHIP_TYPE = 0x00,
  pn557 = 0x01,
  pn81T,
  sn100u,
  sn220u,
  sn300u
} tNFC_chipType;

typedef struct {
    /*Flags common to all chip types*/
    uint8_t _NFCEE_REMOVED_NTF_RECOVERY                     : 1;
    uint8_t _NFCC_FORCE_FW_DOWNLOAD                         : 1;
    uint8_t _NFA_EE_MAX_EE_SUPPORTED                        : 3;
    uint8_t _NFCC_DWNLD_MODE                                : 1;
}tNfc_nfccFeatureList;

typedef struct {
    uint8_t  _NCI_NFCEE_PWR_LINK_CMD                     : 1;
    uint8_t  _ESE_JCOP_DWNLD_PROTECTION                  : 1;
    uint8_t  _ESE_ETSI_READER_ENABLE                     : 1;
}tNfc_eseFeatureList;

typedef struct {
    uint8_t _NCI_INTERFACE_UICC_DIRECT;
    uint8_t _NCI_INTERFACE_ESE_DIRECT;
}tNfc_nfcMwFeatureList;

typedef struct {
    uint8_t nfcNxpEse : 1;
    tNFC_chipType chipType;
    tNfc_nfccFeatureList nfccFL;
    tNfc_eseFeatureList eseFL;
    tNfc_nfcMwFeatureList nfcMwFL;
}tNfc_featureList;

extern tNfc_featureList nfcFL;

#define CONFIGURE_FEATURELIST(chipType)                                       \
  {                                                                           \
    nfcFL.chipType = chipType;                                                \
    switch(chipType){                                                         \
     case pn81T:                                                              \
      nfcFL.chipType = pn557;                                                 \
      FALLTHROUGH_INTENDED;                                                   \
     case sn300u:                                                             \
      FALLTHROUGH_INTENDED;                                                   \
     case sn220u:                                                             \
      FALLTHROUGH_INTENDED;                                                   \
     case sn100u:                                                             \
      nfcFL.nfcNxpEse = true;                                                 \
      CONFIGURE_FEATURELIST_NFCC_WITH_ESE(chipType)                           \
     break;                                                                   \
     default:                                                                 \
      nfcFL.nfcNxpEse = false;                                                \
      CONFIGURE_FEATURELIST_NFCC(chipType)                                    \
     break;                                                                   \
    }                                                                         \
  }

#define CONFIGURE_FEATURELIST_NFCC_WITH_ESE(chipType)                 \
  {                                                                   \
    nfcFL.eseFL._NCI_NFCEE_PWR_LINK_CMD = false;                      \
                                                                      \
    switch(chipType){                                                 \
     case sn220u:                                                     \
      FALLTHROUGH_INTENDED;                                           \
     case sn300u:                                                     \
      FALLTHROUGH_INTENDED;                                           \
     case sn100u:                                                     \
      CONFIGURE_FEATURELIST_NFCC(chipType)                            \
      nfcFL.eseFL._NCI_NFCEE_PWR_LINK_CMD = true;                     \
      nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION = true;                  \
      nfcFL.eseFL._ESE_ETSI_READER_ENABLE = true;                     \
      nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = NFA_EE_MAX_EE_SUPPORTED;\
      break;                                                          \
     case pn81T:                                                      \
      CONFIGURE_FEATURELIST_NFCC(pn557)                               \
      nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION = true;                  \
      nfcFL.eseFL._ESE_ETSI_READER_ENABLE = true;                     \
      nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 4;                      \
     break;                                                           \
     default:                                                         \
     break;                                                           \
    }                                                                 \
  }

#define CONFIGURE_FEATURELIST_NFCC(chipType)                                 \
  {                                                                          \
    nfcFL.eseFL._ESE_ETSI_READER_ENABLE = false;                             \
    nfcFL.eseFL._ESE_JCOP_DWNLD_PROTECTION = false;                          \
                                                                             \
    nfcFL.nfccFL._NFA_EE_MAX_EE_SUPPORTED = 3;                               \
    nfcFL.nfccFL._NFCC_FORCE_FW_DOWNLOAD = true;                             \
    nfcFL.nfccFL._NFCEE_REMOVED_NTF_RECOVERY = true;                         \
                                                                             \
    nfcFL.nfcMwFL._NCI_INTERFACE_UICC_DIRECT = 0x82;                         \
    nfcFL.nfcMwFL._NCI_INTERFACE_ESE_DIRECT = 0x83;                          \
                                                                             \
    switch(chipType){                                                        \
     case sn300u:                                                            \
     FALLTHROUGH_INTENDED;                                                   \
     case sn220u:                                                            \
     FALLTHROUGH_INTENDED;                                                   \
     case sn100u:                                                            \
      nfcFL.nfccFL._NFCC_DWNLD_MODE = NFCC_DWNLD_WITH_NCI_CMD;               \
     break;                                                                  \
     default:                                                                \
     break;                                                                  \
    }                                                                        \
  }
#endif
