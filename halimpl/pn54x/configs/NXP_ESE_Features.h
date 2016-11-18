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

#if(NFC_NXP_ESE == TRUE)

// Reset Schemes
#define NXP_ESE_PN67T_RESET      1
#define NXP_ESE_APDU_GATE_RESET  2
/** Dual/Triple mode priority schemes **/
#define NXP_ESE_EXCLUSIVE_WIRED_MODE    1
#define NXP_ESE_WIRED_MODE_RESUME       2
#define NXP_ESE_WIRED_MODE_TIMEOUT      3

#if(NFC_NXP_CHIP_TYPE == PN547C2)
#define NXP_ESE_WIRED_MODE_DISABLE_DISCOVERY   TRUE
#define NXP_LEGACY_APDU_GATE                   TRUE

#elif((NFC_NXP_CHIP_TYPE == PN548C2) || (NFC_NXP_CHIP_TYPE == PN551))
    #define NFC_NXP_TRIPLE_MODE_PROTECTION        TRUE
    #define NXP_ESE_FELICA_CLT                    FALSE
    #define NXP_WIRED_MODE_STANDBY_PROP           TRUE
    //dual mode prio scheme
    #define NXP_ESE_DUAL_MODE_PRIO_SCHEME         NXP_ESE_WIRED_MODE_TIMEOUT
     //Reset scheme
    #define NXP_ESE_RESET_METHOD                  FALSE
    #define NXP_ESE_ETSI_READER_ENABLE            TRUE
    #define NXP_ESE_SVDD_SYNC                     TRUE
    #define NXP_LEGACY_APDU_GATE                  TRUE
    #define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION   TRUE
    #define NXP_ESE_JCOP_DWNLD_PROTECTION         FALSE

#elif(NFC_NXP_CHIP_TYPE == PN553)
    #define NFC_NXP_TRIPLE_MODE_PROTECTION        FALSE
    #define NXP_ESE_FELICA_CLT                    TRUE
    #define NXP_ESE_WIRED_MODE_PRIO               FALSE
    #define NXP_ESE_UICC_EXCLUSIVE_WIRED_MODE     FALSE    // UICC exclusive wired mode
    //dual mode prio scheme
    #define NXP_ESE_DUAL_MODE_PRIO_SCHEME         NXP_ESE_WIRED_MODE_RESUME
    //reset scheme
    #define NXP_ESE_RESET_METHOD                  TRUE
    #define NXP_ESE_POWER_MODE                    TRUE
    #define NXP_ESE_P73_ISO_RST                   TRUE
    #define NXP_BLOCK_PROPRIETARY_APDU_GATE       FALSE
    #define NXP_WIRED_MODE_STANDBY                TRUE
    #define NXP_ESE_ETSI_READER_ENABLE            TRUE
    #define NXP_ESE_SVDD_SYNC                     FALSE
    #define NXP_LEGACY_APDU_GATE                  FALSE
    #define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION   FALSE
    #define NXP_ESE_JCOP_DWNLD_PROTECTION         TRUE
#endif

#else /*Else of #if(NFC_NXP_ESE == TRUE)*/

#if(NFC_NXP_CHIP_TYPE == PN547C2)
#define NXP_ESE_WIRED_MODE_DISABLE_DISCOVERY      FALSE
#define NXP_LEGACY_APDU_GATE                      FALSE
#endif

#if((NFC_NXP_CHIP_TYPE == PN548C2) || (NFC_NXP_CHIP_TYPE == PN551))
    #define NFC_NXP_TRIPLE_MODE_PROTECTION        FALSE
    #define NXP_WIRED_MODE_STANDBY_PROP           FALSE
    #define NXP_ESE_FELICA_CLT                    FALSE
     //Reset scheme
    #define NXP_ESE_RESET_METHOD                  FALSE
    #define NXP_ESE_ETSI_READER_ENABLE            FALSE
    #define NXP_ESE_SVDD_SYNC                     FALSE
    #define NXP_LEGACY_APDU_GATE                  FALSE
    #define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION   FALSE
    #define NXP_ESE_DUAL_MODE_PRIO_SCHEME         NXP_ESE_WIRED_MODE_TIMEOUT
    #define NXP_ESE_JCOP_DWNLD_PROTECTION         FALSE

#elif(NFC_NXP_CHIP_TYPE == PN553)
    #define NFC_NXP_TRIPLE_MODE_PROTECTION        FALSE
    #define NXP_ESE_FELICA_CLT                    FALSE
    #define NXP_ESE_WIRED_MODE_PRIO               FALSE    //eSE wired mode prio over UICC wired mode
    #define NXP_ESE_UICC_EXCLUSIVE_WIRED_MODE     FALSE   // UICC exclusive wired mode
    //reset scheme
    #define NXP_ESE_RESET_METHOD                  FALSE
    #define NXP_ESE_POWER_MODE                    FALSE
    #define NXP_ESE_P73_ISO_RST                   FALSE
    #define NXP_BLOCK_PROPRIETARY_APDU_GATE       FALSE
    #define NXP_WIRED_MODE_STANDBY                FALSE
    #define NXP_ESE_ETSI_READER_ENABLE            FALSE
    #define NXP_ESE_SVDD_SYNC                     FALSE
    #define NXP_LEGACY_APDU_GATE                  FALSE
    #define NXP_NFCC_ESE_UICC_CONCURRENT_ACCESS_PROTECTION   FALSE
    #define NXP_ESE_DUAL_MODE_PRIO_SCHEME         NXP_ESE_WIRED_MODE_TIMEOUT
    #define NXP_ESE_JCOP_DWNLD_PROTECTION         FALSE
#endif

#endif /*End of #if(NFC_NXP_ESE == TRUE)*/
#endif /*End of #ifndef NXP_ESE_FEATURES_H */
