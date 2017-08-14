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
#include "NxpNfcCapability.h"
#include <phNxpLog.h>

capability* capability::instance = NULL;
tNFC_chipType capability::chipType = pn80T;

capability::capability(){}

capability* capability::getInstance() {
    if (NULL == instance) {
        instance = new capability();
    }
    return instance;
}

tNFC_chipType capability::processChipType(uint8_t* msg, uint16_t msg_len) {
    if((msg != NULL) && (msg_len != 0)) {
        if((msg[0] == 0x60 && msg[1] == 00) ||
                ((offsetFwVersion < msg_len) && (msg[offsetFwVersion] == 0x12))) {
            chipType = pn81T;
        }
        else if(offsetHwVersion < msg_len) {
            ALOGD ("%s HwVersion : 0x%02x", __func__,msg[offsetHwVersion]);
            switch(msg[offsetHwVersion]){

            case 0x40 : //PN553 A0
            case 0x41 : //PN553 B0
                chipType = pn553;
                break;

            case 0x50 : //PN553 A0 + P73
            case 0x51 : //PN553 B0 + P73
                chipType = pn80T;
                break;

            case 0x98 :
                chipType = pn551;
                break;

            case 0xA8 :
                chipType = pn67T;
                break;

            case 0x28 :
                chipType = pn548C2;
                break;

            case 0x18 :
                chipType = pn66T;
                break;

            default :
                chipType = pn80T;
            }
        }
        else {
            ALOGD ("%s Wrong msg_len. Setting Default ChiptType pn80T",__func__);
            chipType = pn80T;
        }
    }
    return chipType;
}

extern "C" tNFC_chipType configChipType(uint8_t* msg, uint16_t msg_len) {
    return pConfigFL->processChipType(msg,msg_len);
}

extern "C" tNFC_chipType getChipType() {
    ALOGD ("%s", __FUNCTION__);
    return capability::chipType;
}


