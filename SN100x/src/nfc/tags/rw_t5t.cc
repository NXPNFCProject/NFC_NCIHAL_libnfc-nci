/******************************************************************************
 *
 *  Copyright (C) 2020 STMicroelectronics
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
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

/******************************************************************************
 *
 *  This file contains the implementation for specific NFC Forum T5T operations
 *  in Reader/Writer mode.
 *
 ******************************************************************************/

#include <android-base/stringprintf.h>
#include <base/logging.h>
#include <log/log.h>
#include <string.h>

#include "bt_types.h"
#include "nfc_api.h"
#include "nfc_int.h"
#include "nfc_target.h"
#include "rw_api.h"
#include "rw_int.h"

using android::base::StringPrintf;

extern void rw_i93_handle_error(tNFC_STATUS);
extern tNFC_STATUS rw_i93_get_next_blocks(uint16_t);
extern tNFC_STATUS rw_i93_send_cmd_read_single_block(uint16_t, bool);
extern tNFC_STATUS rw_i93_send_cmd_write_single_block(uint16_t, uint8_t*);
extern tNFC_STATUS rw_i93_send_cmd_lock_block(uint16_t);

extern bool nfc_debug_enabled;

/*******************************************************************************
**
** Function         rw_t5t_sm_detect_ndef
**
** Description      Process NDEF detection procedure
**
**                  1. Get UID if not having yet
**                  2. Get System Info if not having yet
**                  3. Read first block for CC
**                  4. Search NDEF Type and length
**                  5. Get block status to get max NDEF size and read-only
**                     status
**
** Returns          void
**
*******************************************************************************/
void rw_t5t_sm_detect_ndef(NFC_HDR* p_resp) {
  uint8_t* p = (uint8_t*)(p_resp + 1) + p_resp->offset;
  uint8_t flags, cc[8];
  uint32_t t5t_area_len = 0;
  uint16_t length = p_resp->len, xx;
  tRW_I93_CB* p_i93 = &rw_cb.tcb.i93;
  tRW_DATA rw_data;
  tNFC_STATUS status = NFC_STATUS_FAILED;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s - sub_state:%s (0x%x)", __func__,
      rw_i93_get_sub_state_name(p_i93->sub_state).c_str(), p_i93->sub_state);

  if (length == 0) {
    android_errorWriteLog(0x534e4554, "121260197");
    rw_i93_handle_error(NFC_STATUS_FAILED);
    return;
  }
  STREAM_TO_UINT8(flags, p);
  length--;

  if (flags & I93_FLAG_ERROR_DETECTED) {
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s - Got error flags (0x%02x)", __func__, flags);
    rw_i93_handle_error(NFC_STATUS_FAILED);
    return;
  }

  switch (p_i93->sub_state) {
    case RW_I93_SUBSTATE_WAIT_CC_EXT:

      /* assume block size is 4 */
      STREAM_TO_ARRAY(cc, p, 4);

      status = NFC_STATUS_FAILED;

      /*
      ** Capability Container (CC) second block
      **
      ** CC[4] : RFU
      ** CC[5] : RFU
      ** CC[6] : MSB MLEN
      ** CC[7] : LSB MLEN
      */

      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s - cc[4-7]: 0x%02X 0x%02X 0x%02X 0x%02X", __func__,
                          cc[0], cc[1], cc[2], cc[3]);

      /* T5T_Area length = 8 * MLEN */
      /* CC is 8-byte, MLEN is defined by bytes 6 & 7 */
      t5t_area_len = cc[3] + (cc[2] << 8);
      t5t_area_len <<= 3;
      p_i93->max_ndef_length = t5t_area_len;

      p_i93->t5t_area_last_offset = t5t_area_len + 8 - 1;
      memcpy((uint8_t*)&rw_cb.tcb.i93.gre_cc_content[4], (uint8_t*)cc, 4);
      p_i93->num_block = t5t_area_len / p_i93->block_size;
      p_i93->t5t_area_start_block = 2;

      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "%s - T5T Area size:%d, Nb blocks:0x%04X, Block size:0x%02X, "
          "T5T Area last offset:%d",
          __func__, t5t_area_len, p_i93->num_block, p_i93->block_size,
          p_i93->t5t_area_last_offset);

      status = NFC_STATUS_OK;

      /* search NDEF TLV from offset 8 when CC file coded on 8 bytes NFC Forum
       */
      p_i93->rw_offset = 8;

      if (p_i93->gre_cc_content[0] == I93_ICODE_CC_MAGIC_NUMER_E2) {
        p_i93->intl_flags |= RW_I93_FLAG_EXT_COMMANDS;
      }

      if (rw_i93_get_next_blocks(p_i93->rw_offset) == NFC_STATUS_OK) {
        p_i93->sub_state = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
        p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
      } else {
        rw_i93_handle_error(NFC_STATUS_FAILED);
      }
      break;

    case RW_I93_SUBSTATE_WAIT_CC:

      if (length < RW_I93_CC_SIZE) {
        android_errorWriteLog(0x534e4554, "139188579");
        rw_i93_handle_error(NFC_STATUS_FAILED);
        return;
      }

      /* assume block size is more than RW_I93_CC_SIZE 4 */
      STREAM_TO_ARRAY(cc, p, RW_I93_CC_SIZE);

      status = NFC_STATUS_FAILED;

      /*
      ** Capability Container (CC)
      **
      ** CC[0] : magic number (0xE1)
      ** CC[1] : Bit 7-6:Major version number
      **       : Bit 5-4:Minor version number
      **       : Bit 3-2:Read access condition (00b: read access granted
      **         without any security)
      **       : Bit 1-0:Write access condition (00b: write access granted
      **         without any security)
      ** CC[2] : Memory size in 8 bytes (Ex. 0x04 is 32 bytes) [STM, set to
      **         0xFF if more than 2040bytes]
      ** CC[3] : Bit 0:Read multiple blocks is supported [NXP, STM]
      **       : Bit 1:Inventory page read is supported [NXP]
      **       : Bit 2:More than 2040 bytes are supported [STM]
      */

      DLOG_IF(INFO, nfc_debug_enabled)
          << StringPrintf("%s - cc[0-3]: 0x%02X 0x%02X 0x%02X 0x%02X", __func__,
                          cc[0], cc[1], cc[2], cc[3]);

      if ((cc[0] == I93_ICODE_CC_MAGIC_NUMER_E1) ||
          (cc[0] == I93_ICODE_CC_MAGIC_NUMER_E2)) {
        if ((cc[1] & 0xC0) > I93_VERSION_1_x) {
          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
              "%s - Major mapping version above 1 %d.x", __func__, cc[1] >> 6);
          /* major mapping version above 1 not supported */
          rw_i93_handle_error(NFC_STATUS_FAILED);
          break;
        }

        if ((cc[1] & I93_ICODE_CC_READ_ACCESS_MASK) ==
            I93_ICODE_CC_READ_ACCESS_GRANTED) {
          if ((cc[1] & I93_ICODE_CC_WRITE_ACCESS_MASK) !=
              I93_ICODE_CC_WRITE_ACCESS_GRANTED) {
            /* read-only or password required to write */
            p_i93->intl_flags |= RW_I93_FLAG_READ_ONLY;
          }
          if (cc[3] & I93_ICODE_CC_MBREAD_MASK) {
            /* tag supports read multiple blocks command */
            p_i93->intl_flags |= RW_I93_FLAG_READ_MULTI_BLOCK;
          }

          if (cc[3] & I93_ICODE_CC_SPECIAL_FRAME_MASK) {
            /* tag supports Special Frame for Write-Alike commands */
            p_i93->intl_flags |= RW_I93_FLAG_SPECIAL_FRAME;
          }

          /* get block size from length of the first READ_SINGLE_BLOCK response
           */
          if (length == I93_BLEN_4BYTES)
            p_i93->block_size = I93_BLEN_4BYTES;
          else if (length == I93_BLEN_8BYTES)
            p_i93->block_size = I93_BLEN_8BYTES;
          else if (length == I93_BLEN_16BYTES)
            p_i93->block_size = I93_BLEN_16BYTES;
          else if (length == I93_BLEN_32BYTES)
            p_i93->block_size = I93_BLEN_32BYTES;
          else {
            rw_i93_handle_error(NFC_STATUS_FAILED);
            break;
          }

          /* T5T_Area length = 8 * MLEN */
          if (cc[2] == 0) {
            /* CC is 8-byte, MLEN is defined by bytes 6 & 7 */
            if (length >= I93_BLEN_8BYTES) {
              STREAM_TO_ARRAY(&cc[4], p, 4);
              DLOG_IF(INFO, nfc_debug_enabled)
                  << StringPrintf("%s - cc[4-7]: 0x%02X 0x%02X 0x%02X 0x%02X",
                                  __func__, cc[4], cc[5], cc[6], cc[7]);
              t5t_area_len = cc[7] + (cc[6] << 8);
              t5t_area_len <<= 3;

              p_i93->t5t_area_last_offset = t5t_area_len + 8 - 1;
              p_i93->max_ndef_length = t5t_area_len;
              p_i93->t5t_area_start_block = 1;

              memcpy(p_i93->gre_cc_content, cc, 8);
            } else {
              /* require an additional read to get the second half of the
               * CC from block 1 */
              if (rw_i93_send_cmd_read_single_block(0x0001, false) ==
                  NFC_STATUS_OK) {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_CC_EXT;
              } else {
                rw_i93_handle_error(NFC_STATUS_FAILED);
              }
              memcpy(p_i93->gre_cc_content, cc, 4);
              break;
            }
          } else {
            /* CC is 4-byte, MLEN is defined by byte 2 */
            t5t_area_len = cc[2] << 3;
            p_i93->t5t_area_last_offset = t5t_area_len + 4 - 1;
            p_i93->t5t_area_start_block = 1;

            memcpy(p_i93->gre_cc_content, cc, 4);
          }

          p_i93->num_block = t5t_area_len / p_i93->block_size;

          p_i93->max_ndef_length = t5t_area_len;

          if (cc[0] == I93_ICODE_CC_MAGIC_NUMER_E2) {
            /* can only be done here if CC is coded over 4 bytes */
            p_i93->intl_flags |= RW_I93_FLAG_EXT_COMMANDS;
          }

          DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
              "%s - T5T Area size:%d, Nb blocks:0x%04X, "
              "Block size:0x%02X, T5T Area last offset:%d",
              __func__, t5t_area_len, p_i93->num_block, p_i93->block_size,
              p_i93->t5t_area_last_offset);

          status = NFC_STATUS_OK;
        }
      }

      if (status == NFC_STATUS_OK) {
        /* search NDEF TLV from offset 4 when CC file coded on 4 bytes
         * NFC Forum during the next block reading, if CC file is coded
         * on more than 4 bytes, start searching for NDEF TLV in the current
         * block read except if the CC
         */
        if (cc[2] == 0) {
          /* 8-byte CC length */
          p_i93->rw_offset = 8;

          if (length > I93_BLEN_8BYTES) {
            /* Rest of the block contains T5T_Area, look for NDEF TLV */
            p_i93->sub_state = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
            p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
            /* start TLV search after CC in the first block */
          } else {
            /* 8-byte block length, NDEF TLV search must continue
             * in the next block */
            if (rw_i93_get_next_blocks(p_i93->rw_offset) == NFC_STATUS_OK) {
              p_i93->sub_state = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
              p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
            } else {
              rw_i93_handle_error(NFC_STATUS_FAILED);
            }
            break;
          }
        } else {
          /* 4-byte CC length */
          p_i93->rw_offset = 4;

          if (length > I93_BLEN_4BYTES) {
            /* Rest of the block contains T5T_Area, look for NDEF TLV */
            p_i93->sub_state = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
            p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
            /* start TLV search after CC in the first block */
          } else {
            if (rw_i93_get_next_blocks(p_i93->rw_offset) == NFC_STATUS_OK) {
              p_i93->sub_state = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
              p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
            } else {
              rw_i93_handle_error(NFC_STATUS_FAILED);
            }
            break;
          }
        }
      } else {
        rw_i93_handle_error(NFC_STATUS_FAILED);
        break;
      }

      FALLTHROUGH_INTENDED;

    case RW_I93_SUBSTATE_SEARCH_NDEF_TLV:
      /* search TLV within read blocks */
      for (xx = 0; xx < length; xx++) {
        /* if looking for type */
        if (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_TYPE) {
          if (*(p + xx) == I93_ICODE_TLV_TYPE_NDEF) {
            /* store found type and get length field */
            p_i93->tlv_type = *(p + xx);
            p_i93->ndef_tlv_start_offset = p_i93->rw_offset + xx;

            p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_LENGTH_1;

          } else if (*(p + xx) == I93_ICODE_TLV_TYPE_TERM) {
            /* no NDEF TLV found */
            p_i93->tlv_type = I93_ICODE_TLV_TYPE_TERM;
            break;
          } else {
            /* TLV with Tag field set as RFU are not interpreted */
            p_i93->tlv_type = *(p + xx);
            p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_LENGTH_1;
          }
        } else if (p_i93->tlv_detect_state ==
                   RW_I93_TLV_DETECT_STATE_LENGTH_1) {
          /* if 3 bytes length field */
          if (*(p + xx) == 0xFF) {
            /* need 2 more bytes for length field */
            p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_LENGTH_2;
          } else {
            p_i93->tlv_length = *(p + xx);
            p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_VALUE;

            if (p_i93->tlv_type == I93_ICODE_TLV_TYPE_NDEF) {
              p_i93->ndef_tlv_last_offset =
                  p_i93->ndef_tlv_start_offset + 1 + p_i93->tlv_length;
              break;
            }
          }
        } else if (p_i93->tlv_detect_state ==
                   RW_I93_TLV_DETECT_STATE_LENGTH_2) {
          /* the second byte of 3 bytes length field */
          p_i93->tlv_length = *(p + xx);
          p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_LENGTH_3;

        } else if (p_i93->tlv_detect_state ==
                   RW_I93_TLV_DETECT_STATE_LENGTH_3) {
          /* the last byte of 3 bytes length field */
          p_i93->tlv_length = (p_i93->tlv_length << 8) + *(p + xx);
          p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_VALUE;

          if (p_i93->tlv_type == I93_ICODE_TLV_TYPE_NDEF) {
            p_i93->ndef_tlv_last_offset =
                p_i93->ndef_tlv_start_offset + 3 + p_i93->tlv_length;
            break;
          }
        } else if (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_VALUE) {
          /* this is other than NDEF TLV */
          if (p_i93->tlv_length <= length - xx) {
            /* skip value field */
            xx += (uint8_t)p_i93->tlv_length - 1;
            p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
          } else {
            /* read more data */
            p_i93->tlv_length -= (length - xx);
            break;
          }
        }
      }

      /* found NDEF TLV and read length field */
      if ((p_i93->tlv_type == I93_ICODE_TLV_TYPE_NDEF) &&
          (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_VALUE)) {
        p_i93->ndef_length = p_i93->tlv_length;

        rw_data.ndef.status = NFC_STATUS_OK;
        rw_data.ndef.protocol = NFC_PROTOCOL_T5T;
        rw_data.ndef.flags = 0;
        rw_data.ndef.flags |= RW_NDEF_FL_SUPPORTED;
        rw_data.ndef.flags |= RW_NDEF_FL_FORMATED;  // NOTYPO
        rw_data.ndef.flags |= RW_NDEF_FL_FORMATABLE;
        rw_data.ndef.cur_size = p_i93->ndef_length;

        if (p_i93->intl_flags & RW_I93_FLAG_READ_ONLY) {
          rw_data.ndef.flags |= RW_NDEF_FL_READ_ONLY;
          rw_data.ndef.max_size = p_i93->ndef_length;
        } else {
          rw_data.ndef.flags |= RW_NDEF_FL_HARD_LOCKABLE;
          /* Assume tag is already in INITIALIZED or READ-WRITE state */
          p_i93->max_ndef_length =
              p_i93->t5t_area_last_offset - p_i93->ndef_tlv_start_offset + 1;
          if (p_i93->max_ndef_length >= 0xFF) {
            /* 3 bytes length field can be used */
            p_i93->max_ndef_length -= 4;
          } else {
            /* 1 byte length field can be used */
            p_i93->max_ndef_length -= 2;
          }
          rw_data.ndef.max_size = p_i93->max_ndef_length;
        }

        /* Complete the greedy collection */
        p_i93->gre_ndef_tlv_pos = p_i93->ndef_tlv_start_offset;
        p_i93->gre_ndef_tlv_length = p_i93->ndef_length;
        p_i93->gre_validity = 1;
        p_i93->state = RW_I93_STATE_IDLE;
        p_i93->sent_cmd = 0;

        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
            "%s - NDEF cur_size (%d), max_size (%d), flags (0x%x)", __func__,
            rw_data.ndef.cur_size, rw_data.ndef.max_size, rw_data.ndef.flags);

        (*(rw_cb.p_cback))(RW_I93_NDEF_DETECT_EVT, &rw_data);
        break;
      } else {
        /* read more data */
        p_i93->rw_offset += length;

        if (p_i93->rw_offset > p_i93->t5t_area_last_offset) {
          rw_i93_handle_error(NFC_STATUS_FAILED);
        } else {
          if (rw_i93_get_next_blocks(p_i93->rw_offset) == NFC_STATUS_OK) {
            p_i93->sub_state = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
          } else {
            rw_i93_handle_error(NFC_STATUS_FAILED);
          }
        }
      }
      break;

    default:
      break;
  }
}

/*******************************************************************************
**
** Function         rw_t5t_sm_update_ndef
**
** Description      Process NDEF update procedure
**
**                  1. Set length field to zero
**                  2. Write NDEF and Terminator TLV
**                  3. Set length field to NDEF length
**
** Returns          void
**
*******************************************************************************/
void rw_t5t_sm_update_ndef(NFC_HDR* p_resp) {
  uint8_t* p = (uint8_t*)(p_resp + 1) + p_resp->offset;
  uint8_t flags, xx, buff[I93_MAX_BLOCK_LENGH];
  uint16_t length_offset;
  uint16_t length = p_resp->len, block_number;
  tRW_I93_CB* p_i93 = &rw_cb.tcb.i93;
  tRW_DATA rw_data;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s - sub_state:%s (0x%x)", __func__,
      rw_i93_get_sub_state_name(p_i93->sub_state).c_str(), p_i93->sub_state);

  if (length == 0 || p_i93->block_size > I93_MAX_BLOCK_LENGH) {
    android_errorWriteLog(0x534e4554, "122320256");
    rw_i93_handle_error(NFC_STATUS_FAILED);
    return;
  }

  STREAM_TO_UINT8(flags, p);
  length--;

  if (flags & I93_FLAG_ERROR_DETECTED) {
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s - Got error flags (0x%02x)", __func__, flags);
    rw_i93_handle_error(NFC_STATUS_FAILED);
    return;
  }

  switch (p_i93->sub_state) {
    case RW_I93_SUBSTATE_RESET_LEN:

      /* get offset of length field */
      length_offset = (p_i93->ndef_tlv_start_offset + 1) % p_i93->block_size;

      if (length < length_offset) {
        android_errorWriteLog(0x534e4554, "122320256");
        rw_i93_handle_error(NFC_STATUS_FAILED);
        return;
      }

      /* set length to zero */
      *(p + length_offset) = 0x00;

      if (p_i93->ndef_length > 0) {
        /* if 3 bytes length field is needed */
        if (p_i93->ndef_length >= 0xFF) {
          xx = length_offset + 3;
        } else {
          xx = length_offset + 1;
        }

        if (p_i93->ndef_length >= 0xFF) {
          p_i93->ndef_tlv_last_offset = p_i93->ndef_tlv_start_offset + 3;
        } else {
          p_i93->ndef_tlv_last_offset = p_i93->ndef_tlv_start_offset + 1;
        }

        /* write the first part of NDEF in the same block */
        for (; xx < p_i93->block_size; xx++) {
          if (xx > length || p_i93->rw_length > p_i93->ndef_length) {
            android_errorWriteLog(0x534e4554, "122320256");
            rw_i93_handle_error(NFC_STATUS_FAILED);
            return;
          }
          if (p_i93->rw_length < p_i93->ndef_length) {
            *(p + xx) = *(p_i93->p_update_data + p_i93->rw_length++);
            p_i93->ndef_tlv_last_offset++;
          } else {
            *(p + xx) = I93_ICODE_TLV_TYPE_NULL;
          }
        }
      }

      block_number = (p_i93->ndef_tlv_start_offset + 1) / p_i93->block_size;

      if (rw_i93_send_cmd_write_single_block(block_number, p) ==
          NFC_STATUS_OK) {
        /* update next writing offset */
        p_i93->rw_offset = (block_number + 1) * p_i93->block_size;
        p_i93->sub_state = RW_I93_SUBSTATE_WRITE_NDEF;
      } else {
        rw_i93_handle_error(NFC_STATUS_FAILED);
      }
      break;

    case RW_I93_SUBSTATE_WRITE_NDEF:

      /* if it's not the end of tag memory */
      if (p_i93->rw_offset <= p_i93->t5t_area_last_offset) {
        // Note: Needed to write in the last block of the memory
        block_number = p_i93->rw_offset / p_i93->block_size;

        /* if we have more data to write */
        if (p_i93->rw_length < p_i93->ndef_length) {
          p = p_i93->p_update_data + p_i93->rw_length;

          p_i93->rw_offset += p_i93->block_size;
          p_i93->rw_length += p_i93->block_size;

          /* if this is the last block of NDEF TLV */
          if (p_i93->rw_length >= p_i93->ndef_length) {
            /* length of NDEF TLV in the block */
            xx = (uint8_t)(p_i93->block_size -
                           (p_i93->rw_length - p_i93->ndef_length));
            /* set NULL TLV in the unused part of block */
            memset(buff, I93_ICODE_TLV_TYPE_NULL, p_i93->block_size);
            memcpy(buff, p, xx);
            p = buff;

            /* if it's the end of tag memory */
            if (((p_i93->rw_offset - p_i93->block_size + xx) <=
                 p_i93->t5t_area_last_offset) &&
                (xx < p_i93->block_size)) {
              buff[xx] = I93_ICODE_TLV_TYPE_TERM;
            }
            // Note: Needed to write Terminator TLV in block following
            // the NDEF Message ending in byte 3 of the previous block
            p_i93->ndef_tlv_last_offset =
                p_i93->rw_offset - p_i93->block_size + xx - 1;

          } else {
            p_i93->ndef_tlv_last_offset += p_i93->block_size;
          }

          if (rw_i93_send_cmd_write_single_block(block_number, p) !=
              NFC_STATUS_OK) {
            rw_i93_handle_error(NFC_STATUS_FAILED);
          }
        } else {
          /* if this is the very next block of NDEF TLV */
          // Note: Needed to write Terminator TLV in block following
          // the NDEF Message ending in byte 3 of the previous block
          if ((block_number ==
               (p_i93->ndef_tlv_last_offset / p_i93->block_size) + 1) &&
              (((p_i93->ndef_tlv_last_offset + 1) % p_i93->block_size) == 0)) {
            /* write Terminator TLV and NULL TLV */
            memset(buff, I93_ICODE_TLV_TYPE_NULL, p_i93->block_size);
            buff[0] = I93_ICODE_TLV_TYPE_TERM;
            p = buff;

            if (rw_i93_send_cmd_write_single_block(block_number, p) !=
                NFC_STATUS_OK) {
              rw_i93_handle_error(NFC_STATUS_FAILED);
            }

            block_number =
                (p_i93->ndef_tlv_start_offset + 1) / p_i93->block_size;

            /* set offset to length field */
            p_i93->rw_offset = p_i93->ndef_tlv_start_offset + 1;

            /* get size of length field */
            if (p_i93->ndef_length >= 0xFF) {
              p_i93->rw_length = 3;
            } else if (p_i93->ndef_length > 0) {
              p_i93->rw_length = 1;
            } else {
              p_i93->rw_length = 0;
            }
            p_i93->sub_state = RW_I93_SUBSTATE_UPDATE_LEN;

          } else {
            /* finished writing NDEF and Terminator TLV */
            /* read length field to update length       */
            block_number =
                (p_i93->ndef_tlv_start_offset + 1) / p_i93->block_size;

            if (rw_i93_send_cmd_read_single_block(block_number, false) ==
                NFC_STATUS_OK) {
              /* set offset to length field */
              p_i93->rw_offset = p_i93->ndef_tlv_start_offset + 1;

              /* get size of length field */
              if (p_i93->ndef_length >= 0xFF) {
                p_i93->rw_length = 3;
              } else if (p_i93->ndef_length > 0) {
                p_i93->rw_length = 1;
              } else {
                p_i93->rw_length = 0;
              }
              p_i93->sub_state = RW_I93_SUBSTATE_UPDATE_LEN;
            } else {
              rw_i93_handle_error(NFC_STATUS_FAILED);
            }
          }
        }
      } /* (p_i93->rw_offset <= p_i93->t5t_area_last_offset) */

      else {
        /* if we have no more data to write */
        if (p_i93->rw_length >= p_i93->ndef_length) {
          /* finished writing NDEF and Terminator TLV */
          /* read length field to update length       */
          block_number = (p_i93->ndef_tlv_start_offset + 1) / p_i93->block_size;

          if (rw_i93_send_cmd_read_single_block(block_number, false) ==
              NFC_STATUS_OK) {
            /* set offset to length field */
            p_i93->rw_offset = p_i93->ndef_tlv_start_offset + 1;

            /* get size of length field */
            if (p_i93->ndef_length >= 0xFF) {
              p_i93->rw_length = 3;
            } else if (p_i93->ndef_length > 0) {
              p_i93->rw_length = 1;
            } else {
              p_i93->rw_length = 0;
            }

            p_i93->sub_state = RW_I93_SUBSTATE_UPDATE_LEN;
            break;
          }
        }
        rw_i93_handle_error(NFC_STATUS_FAILED);
      }
      break;

    case RW_I93_SUBSTATE_UPDATE_LEN:
      /* if we have more length field to write */
      if (p_i93->rw_length > 0) {
        /* if we got ack for writing, read next block to update rest of length
         * field */
        if (length == 0) {
          block_number = p_i93->rw_offset / p_i93->block_size;

          if (rw_i93_send_cmd_read_single_block(block_number, false) !=
              NFC_STATUS_OK) {
            rw_i93_handle_error(NFC_STATUS_FAILED);
          }
        } else {
          length_offset = p_i93->rw_offset % p_i93->block_size;

          /* update length field within the read block */
          for (xx = length_offset; xx < p_i93->block_size; xx++) {
            if (xx > length) {
              android_errorWriteLog(0x534e4554, "122320256");
              rw_i93_handle_error(NFC_STATUS_FAILED);
              return;
            }

            if (p_i93->rw_length == 3)
              *(p + xx) = 0xFF;
            else if (p_i93->rw_length == 2)
              *(p + xx) = (uint8_t)((p_i93->ndef_length >> 8) & 0xFF);
            else if (p_i93->rw_length == 1)
              *(p + xx) = (uint8_t)(p_i93->ndef_length & 0xFF);

            p_i93->rw_length--;
            if (p_i93->rw_length == 0) break;
          }

          block_number = (p_i93->rw_offset / p_i93->block_size);

          if (rw_i93_send_cmd_write_single_block(block_number, p) ==
              NFC_STATUS_OK) {
            /* set offset to the beginning of next block */
            p_i93->rw_offset +=
                p_i93->block_size - (p_i93->rw_offset % p_i93->block_size);
          } else {
            rw_i93_handle_error(NFC_STATUS_FAILED);
          }
        }
      } else {
        DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
            "%s - NDEF update complete, %d bytes, (%d-%d)", __func__,
            p_i93->ndef_length, p_i93->ndef_tlv_start_offset,
            p_i93->ndef_tlv_last_offset);

        p_i93->state = RW_I93_STATE_IDLE;
        p_i93->sent_cmd = 0;
        p_i93->p_update_data = nullptr;

        rw_data.status = NFC_STATUS_OK;
        (*(rw_cb.p_cback))(RW_I93_NDEF_UPDATE_CPLT_EVT, &rw_data);
      }
      break;

    default:
      break;
  }
}

/*******************************************************************************
**
** Function         rw_i93_sm_set_read_only
**
** Description      Process read-only procedure
**
**                  1. Update CC as read-only
**                  2. Lock all block of NDEF TLV
**                  3. Lock block of CC
**
** Returns          void
**
*******************************************************************************/
void rw_t5t_sm_set_read_only(NFC_HDR* p_resp) {
  uint8_t* p = (uint8_t*)(p_resp + 1) + p_resp->offset;
  uint16_t block_number;
  uint8_t flags;

  uint16_t length = p_resp->len;
  tRW_I93_CB* p_i93 = &rw_cb.tcb.i93;
  tRW_DATA rw_data;

  DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
      "%s - sub_state:%s (0x%x)", __func__,
      rw_i93_get_sub_state_name(p_i93->sub_state).c_str(), p_i93->sub_state);

  if (length == 0) {
    android_errorWriteLog(0x534e4554, "122322613");
    rw_i93_handle_error(NFC_STATUS_FAILED);
    return;
  }

  STREAM_TO_UINT8(flags, p);
  length--;

  if (flags & I93_FLAG_ERROR_DETECTED) {
    DLOG_IF(INFO, nfc_debug_enabled)
        << StringPrintf("%s - Got error flags (0x%02x)", __func__, flags);
    rw_i93_handle_error(NFC_STATUS_FAILED);
    return;
  }

  switch (p_i93->sub_state) {
    case RW_I93_SUBSTATE_WAIT_CC:

      if (length < RW_I93_CC_SIZE) {
        android_errorWriteLog(0x534e4554, "139188579");
        rw_i93_handle_error(NFC_STATUS_FAILED);
        return;
      }
      /* mark CC as read-only */
      *(p + 1) |= I93_ICODE_CC_READ_ONLY;

      if (rw_i93_send_cmd_write_single_block(0, p) == NFC_STATUS_OK) {
        p_i93->sub_state = RW_I93_SUBSTATE_WAIT_UPDATE_CC;
      } else {
        rw_i93_handle_error(NFC_STATUS_FAILED);
      }
      break;

    case RW_I93_SUBSTATE_WAIT_UPDATE_CC:

      /* successfully write CC then lock CC and all blocks of T5T Area */
      p_i93->rw_offset = 0;

      if (rw_i93_send_cmd_lock_block(0) == NFC_STATUS_OK) {
        p_i93->rw_offset += p_i93->block_size;
        p_i93->sub_state = RW_I93_SUBSTATE_LOCK_T5T_AREA;
      } else {
        rw_i93_handle_error(NFC_STATUS_FAILED);
      }
      break;

    case RW_I93_SUBSTATE_LOCK_T5T_AREA:

      DLOG_IF(INFO, nfc_debug_enabled) << StringPrintf(
          "%s - rw_offset:0x%02x, t5t_area_last_offset:0x%02x", __func__,
          p_i93->rw_offset, p_i93->t5t_area_last_offset);

      /* 2nd block to be locked can be the last 4 bytes of CC in case CC
       * is 8byte long, then T5T_Area starts */
      if (p_i93->rw_offset <= p_i93->t5t_area_last_offset) {
        if (p_i93->block_size == 0) {
          LOG(ERROR) << StringPrintf("%s - zero block_size error", __func__);
          rw_i93_handle_error(NFC_STATUS_FAILED);
          break;
        }
        /* get the next block of NDEF TLV */
        block_number = (uint16_t)(p_i93->rw_offset / p_i93->block_size);

        if (rw_i93_send_cmd_lock_block(block_number) == NFC_STATUS_OK) {
          p_i93->rw_offset += p_i93->block_size;
        } else {
          rw_i93_handle_error(NFC_STATUS_FAILED);
        }
      } else {
        p_i93->intl_flags |= RW_I93_FLAG_READ_ONLY;
        p_i93->state = RW_I93_STATE_IDLE;
        p_i93->sent_cmd = 0;

        rw_data.status = NFC_STATUS_OK;
        (*(rw_cb.p_cback))(RW_I93_SET_TAG_RO_EVT, &rw_data);
      }
      break;

    default:
      break;
  }
}
