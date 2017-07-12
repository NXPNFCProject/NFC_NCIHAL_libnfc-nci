/*
 * Copyright (C) 2012-2016 NXP Semiconductors
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
 * NXP ESE features macros definitions
 */

#ifndef NXP_ESE_FEATURES_H
#define NXP_ESE_FEATURES_H

/** Dual/Triple mode priority schemes **/
#define NXP_ESE_EXCLUSIVE_WIRED_MODE 1
#define NXP_ESE_WIRED_MODE_RESUME 2
#define NXP_ESE_WIRED_MODE_TIMEOUT 3

/*Chip ID specific macros as per configurations file*/
#define CHIP_ID_PN547C2 0x01
#define CHIP_ID_PN65T 0x02
#define CHIP_ID_PN548AD 0x03
#define CHIP_ID_PN66T 0x04
#define CHIP_ID_PN551 0x05
#define CHIP_ID_PN67T 0x06
#define CHIP_ID_PN553 0x07
#define CHIP_ID_PN80T 0x08
#define CHIP_ID_PN557 0x09
#define CHIP_ID_PN81T 0x0A

#if (NFC_NXP_ESE == TRUE)
// Reset Schemes
#define NXP_ESE_PN67T_RESET 1
#define NXP_ESE_APDU_GATE_RESET 2

#if (NFC_NXP_CHIP_TYPE == PN547C2)
#define NXP_ESE_WIRED_MODE_DISABLE_DISCOVERY true
#define NXP_LEGACY_APDU_GATE true

#elif((NFC_NXP_CHIP_TYPE == PN548C2) || (NFC_NXP_CHIP_TYPE == PN551))
#define NFC_NXP_TRIPLE_MODE_PROTECTION true
#define NXP_ESE_FELICA_CLT false
#define NXP_WIRED_MODE_STANDBY_PROP true
#define NXP_WIRED_MODE_STANDBY false
// dual mode prio scheme
#define NXP_ESE_DUAL_MODE_PRIO_SCHEME NXP_ESE_WIRED_MODE_TIMEOUT
// Reset scheme
#define NXP_ESE_RESET_METHOD false
#define NXP_ESE_ETSI_READER_ENABLE true
#define NXP_ESE_SVDD_SYNC true
#define NXP_LEGACY_APDU_GATE true
#define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION true
#define NXP_ESE_JCOP_DWNLD_PROTECTION false
#define NXP_UICC_HANDLE_CLEAR_ALL_PIPES false
#define NFC_NXP_GP_CONTINOUS_PROCESSING false
#define NXP_ESE_DWP_SPI_SYNC_ENABLE true
#define NFC_NXP_ESE_ETSI12_PROP_INIT false

#elif(NFC_NXP_CHIP_TYPE == PN553)

#define NFC_NXP_TRIPLE_MODE_PROTECTION false
#define NXP_ESE_FELICA_CLT true
#define NXP_ESE_WIRED_MODE_PRIO false
#define NXP_ESE_UICC_EXCLUSIVE_WIRED_MODE false  // UICC exclusive wired mode
// dual mode prio scheme
#define NXP_ESE_DUAL_MODE_PRIO_SCHEME NXP_ESE_WIRED_MODE_RESUME
// reset scheme
#define NXP_ESE_RESET_METHOD true
#define NXP_ESE_POWER_MODE true
#define NXP_ESE_P73_ISO_RST true
#define NXP_BLOCK_PROPRIETARY_APDU_GATE false
#define NXP_WIRED_MODE_STANDBY true
#define NXP_WIRED_MODE_STANDBY_PROP false
#define NXP_ESE_ETSI_READER_ENABLE true
#define NXP_ESE_SVDD_SYNC true
#define NXP_LEGACY_APDU_GATE false
#define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION false
#define NXP_ESE_JCOP_DWNLD_PROTECTION true
#define NXP_UICC_HANDLE_CLEAR_ALL_PIPES true
#define NFC_NXP_GP_CONTINOUS_PROCESSING false
#define NXP_ESE_DWP_SPI_SYNC_ENABLE true
#define NFC_NXP_ESE_ETSI12_PROP_INIT true

#elif(NFC_NXP_CHIP_TYPE == PN557)

#define NFC_NXP_TRIPLE_MODE_PROTECTION false
#define NXP_ESE_FELICA_CLT true
#define NXP_ESE_WIRED_MODE_PRIO false
#define NXP_ESE_UICC_EXCLUSIVE_WIRED_MODE false  // UICC exclusive wired mode
// dual mode prio scheme
#define NXP_ESE_DUAL_MODE_PRIO_SCHEME NXP_ESE_WIRED_MODE_RESUME
// reset scheme
#define NXP_ESE_RESET_METHOD true
#define NXP_ESE_POWER_MODE true
#define NXP_ESE_P73_ISO_RST true
#define NXP_BLOCK_PROPRIETARY_APDU_GATE false
#define NXP_WIRED_MODE_STANDBY true
#define NXP_WIRED_MODE_STANDBY_PROP false
#define NXP_ESE_ETSI_READER_ENABLE true
#define NXP_ESE_SVDD_SYNC true
#define NXP_LEGACY_APDU_GATE false
#define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION false
#define NXP_ESE_JCOP_DWNLD_PROTECTION true
#define NXP_UICC_HANDLE_CLEAR_ALL_PIPES true
#define NFC_NXP_GP_CONTINOUS_PROCESSING false
#define NXP_ESE_DWP_SPI_SYNC_ENABLE true
#define NFC_NXP_ESE_ETSI12_PROP_INIT true

#endif

#else /*Else of #if(NFC_NXP_ESE == TRUE)*/

#if (NFC_NXP_CHIP_TYPE == PN547C2)
#define NXP_ESE_WIRED_MODE_DISABLE_DISCOVERY false
#define NXP_LEGACY_APDU_GATE false
#endif

#if ((NFC_NXP_CHIP_TYPE == PN548C2) || (NFC_NXP_CHIP_TYPE == PN551))
#define NFC_NXP_TRIPLE_MODE_PROTECTION false
#define NXP_WIRED_MODE_STANDBY_PROP false
#define NXP_ESE_FELICA_CLT false
// Reset scheme
#define NXP_ESE_RESET_METHOD false
#define NXP_ESE_ETSI_READER_ENABLE false
#define NXP_ESE_SVDD_SYNC false
#define NXP_ESE_DWP_SPI_SYNC_ENABLE false
#define NXP_LEGACY_APDU_GATE false
#define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION false
#define NXP_ESE_DUAL_MODE_PRIO_SCHEME NXP_ESE_WIRED_MODE_TIMEOUT
#define NXP_ESE_JCOP_DWNLD_PROTECTION false
#define NXP_UICC_HANDLE_CLEAR_ALL_PIPES false
#define NFC_NXP_ESE_ETSI12_PROP_INIT false
#define NFC_NXP_GP_CONTINOUS_PROCESSING false
#elif(NFC_NXP_CHIP_TYPE == PN553)

#define NFC_NXP_TRIPLE_MODE_PROTECTION false
#define NXP_ESE_FELICA_CLT false
#define NXP_ESE_WIRED_MODE_PRIO \
  false  // eSE wired mode prio over UICC wired mode
#define NXP_ESE_UICC_EXCLUSIVE_WIRED_MODE false  // UICC exclusive wired mode
// reset scheme
#define NXP_ESE_RESET_METHOD false
#define NXP_ESE_POWER_MODE false
#define NXP_ESE_P73_ISO_RST false
#define NXP_BLOCK_PROPRIETARY_APDU_GATE false
#define NXP_WIRED_MODE_STANDBY false
#define NXP_ESE_ETSI_READER_ENABLE false
#define NXP_ESE_SVDD_SYNC false
#define NXP_LEGACY_APDU_GATE false
#define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION false
#define NXP_ESE_DWP_SPI_SYNC_ENABLE false
#define NXP_ESE_DUAL_MODE_PRIO_SCHEME NXP_ESE_WIRED_MODE_TIMEOUT
#define NXP_ESE_JCOP_DWNLD_PROTECTION false
#define NXP_UICC_HANDLE_CLEAR_ALL_PIPES false
#define NFC_NXP_ESE_ETSI12_PROP_INIT true
#define NFC_NXP_GP_CONTINOUS_PROCESSING false

#elif(NFC_NXP_CHIP_TYPE == PN557)

#define NFC_NXP_TRIPLE_MODE_PROTECTION false
#define NXP_ESE_FELICA_CLT false
#define NXP_ESE_WIRED_MODE_PRIO \
  false  // eSE wired mode prio over UICC wired mode
#define NXP_ESE_UICC_EXCLUSIVE_WIRED_MODE false  // UICC exclusive wired mode
// reset scheme
#define NXP_ESE_RESET_METHOD false
#define NXP_ESE_POWER_MODE false
#define NXP_ESE_P73_ISO_RST false
#define NXP_BLOCK_PROPRIETARY_APDU_GATE false
#define NXP_WIRED_MODE_STANDBY false
#define NXP_ESE_ETSI_READER_ENABLE false
#define NXP_ESE_SVDD_SYNC false
#define NXP_LEGACY_APDU_GATE false
#define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION false
#define NXP_ESE_DWP_SPI_SYNC_ENABLE false
#define NXP_ESE_DUAL_MODE_PRIO_SCHEME NXP_ESE_WIRED_MODE_TIMEOUT
#define NXP_ESE_JCOP_DWNLD_PROTECTION false
#define NXP_UICC_HANDLE_CLEAR_ALL_PIPES false
#define NFC_NXP_ESE_ETSI12_PROP_INIT true
#define NFC_NXP_GP_CONTINOUS_PROCESSING false

#endif

#endif /*End of #if(NFC_NXP_ESE == TRUE)*/
#endif /*End of #ifndef NXP_ESE_FEATURES_H */
