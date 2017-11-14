/******************************************************************************
 *
 *  Copyright (C) 2015 NXP Semiconductors
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
#include "Nxp_Features.h"
#ifndef __CAP_H__
#define __CAP_H__
#define pConfigFL       (capability::getInstance())
#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
**
** Function         getChipType
**
** Description      Gets the chipType which is configured during bootup
**
** Parameters       none
**
** Returns          chipType
*******************************************************************************/
tNFC_chipType getChipType ();

/*******************************************************************************
**
** Function         configChipType
**
** Description      Finds chiptType by processing msg buffer
**
** Parameters       msg, len
**                  msg : CORE_INIT_RESPONSE (NCI 1.0)
**                           CORE_RST_NTF (NCI 2.0)
**
** Returns          chipType
*******************************************************************************/
tNFC_chipType configChipType(uint8_t* msg, uint16_t msg_len);
#ifdef __cplusplus
};

class capability {
private:
    static capability* instance;
    const uint16_t offsetHwVersion = 24;
    const uint16_t offsetFwVersion = 25;
    /*product[] will be used to print product version and
    should be kept in accordance with tNFC_chipType*/
    const char* product[11] = {"UNKNOWN","PN547C2","PN65T","PN548C2","PN66T",
    "PN551","PN67T","PN553","PN80T","PN557","PN81T"};
    capability();
public:
    static tNFC_chipType chipType;
    static capability* getInstance();
    tNFC_chipType processChipType(uint8_t* msg, uint16_t msg_len);
};

#endif

#endif

