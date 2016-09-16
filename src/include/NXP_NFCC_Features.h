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
 * NXP NFCC features macros definitions
 */

#ifndef NXP_NFCC_FEATURES_H
#define NXP_NFCC_FEATURES_H
#define NXP_NFCC_EMPTY_DATA_PACKET TRUE
#if(NFC_NXP_CHIP_TYPE == PN553)
// Example: define macro like below for each feature
//#define NXP_NFCC_FEATURE_1  TRUE
#define NXP_NFCC_MW_RCVRY_BLK_FW_DNLD   TRUE
#define NXP_NFCC_DYNAMIC_DUAL_UICC      TRUE
#define NXP_NFCC_FW_WA                  TRUE
#define NXP_NFCC_ROUTING_BLOCK_BIT      TRUE
#elif((NFC_NXP_CHIP_TYPE == PN551)||(NFC_NXP_CHIP_TYPE == PN548C2))
#define NXP_NFCC_DYNAMIC_DUAL_UICC      FALSE
#define NXP_NFCC_ROUTING_BLOCK_BIT_PROP TRUE
#endif
#endif                          /* end of #ifndef NXP_NFCC_FEATURES_H */
