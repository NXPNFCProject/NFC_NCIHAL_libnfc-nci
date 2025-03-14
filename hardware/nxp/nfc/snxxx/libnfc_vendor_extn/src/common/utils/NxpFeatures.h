/**
 *
 *  Copyright 2025 NXP
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
 **/
#ifndef NXP_FEATURES_H
#define NXP_FEATURES_H

typedef enum {
  DEFAULT_CHIP_TYPE = 0x00,
  pn547C2 = 0x01,
  pn65T,
  pn548C2,
  pn66T,
  pn551,
  pn67T,
  pn553,
  pn80T,
  pn557,
  pn81T,
  sn100u,
  sn220u,
  pn560,
  sn300u
} tNFC_chipType;

typedef enum {
  HW_PN553_A0 = 0x40,
  HW_PN553_B0 = 0x41,
  HW_PN80T_A0 = 0x50,
  HW_PN80T_B0 = 0x51, // PN553 B0 + P73
  HW_PN551 = 0x98,
  HW_PN67T_A0 = 0xA8,
  HW_PN67T_B0 = 0x08,
  HW_PN548C2_A0 = 0x28,
  HW_PN548C2_B0 = 0x48,
  HW_PN66T_A0 = 0x18,
  HW_PN66T_B0 = 0x58,
  HW_SN100U_A0 = 0xA0,
  HW_SN100U_A2 = 0xA2,
  HW_PN560_V1 = 0xCA,
  HW_PN560_V2 = 0xCB,
  HW_SN300U = 0xD3
} tNFC_HWVersion;

#define NCI_CMD_RSP_SUCCESS_SW1 0x60
#define NCI_CMD_RSP_SUCCESS_SW2 0x00

#define GET_FW_ROM_VERSION_NCI_RESP(msg, msg_len) (msg[msg_len - 3])
#define GET_FW_MAJOR_VERSION_NCI_RESP(msg, msg_len) (msg[msg_len - 2])
#define GET_HW_VERSION_NCI_RESP(msg, msg_len) (msg[msg_len - 4])

/*FW ROM CODE VERSION*/
#define FW_MOBILE_ROM_VERSION_PN553 0x11
#define FW_MOBILE_ROM_VERSION_PN557 0x12
#define FW_MOBILE_ROM_VERSION_SN100U 0x01
#define FW_MOBILE_ROM_VERSION_SN220U 0x01
#define FW_MOBILE_ROM_VERSION_SN300U 0x02

/*FW Major VERSION Code*/
#define FW_MOBILE_MAJOR_NUMBER_PN553 0x01
#define FW_MOBILE_MAJOR_NUMBER_PN557 0x01
#define FW_MOBILE_MAJOR_NUMBER_PN557_V2 0x21
#define FW_MOBILE_MAJOR_NUMBER_SN100U 0x010
#define FW_MOBILE_MAJOR_NUMBER_SN220U 0x01
#define FW_MOBILE_MAJOR_NUMBER_SN300U 0x20

/** @}*/
#endif // NXP_FEATURES_H
