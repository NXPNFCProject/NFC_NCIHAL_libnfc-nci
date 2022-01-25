/**
 * Copyright (C) 2021 The Android Open Source Project
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

#include "include/debug_lmrt.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

using android::base::StringPrintf;

extern bool nfc_debug_enabled;

/* The payload of each RF_SET_LISTEN_MODE_ROUTING_CMD when commit routing */
lmrt_payload_t lmrt_payloads;

/* The committed routing table stored in tlv form  */
std::vector<uint8_t> committed_lmrt_tlvs(0);

/*******************************************************************************
**
** Function         debug_lmrt_init
**
** Description      initialize the lmrt_payloads
**
** Returns          None
**
*******************************************************************************/
void debug_lmrt_init(void) {
  std::vector<uint8_t> empty_more(0);
  std::vector<uint8_t> empty_entry_count(0);
  std::vector<std::vector<uint8_t>> empty_tlvs(0);

  lmrt_payloads.more.swap(empty_more);
  lmrt_payloads.entry_count.swap(empty_entry_count);
  lmrt_payloads.tlvs.swap(empty_tlvs);
}

/*******************************************************************************
**
** Function         lmrt_log
**
** Description      print the listen mode routing configuration for debug use
**
** Returns          None
**
*******************************************************************************/
void lmrt_log(void) {
  if (!nfc_debug_enabled) return;

  static const char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  for (int i = 0; i < lmrt_payloads.more.size(); i++) {
    std::string tlvs_str;
    for (uint8_t byte : lmrt_payloads.tlvs[i]) {
      tlvs_str.push_back(hexmap[byte >> 4]);
      tlvs_str.push_back(hexmap[byte & 0x0F]);
    }

    LOG(INFO) << StringPrintf("lmrt_log: Packet %d/%d, %d more packet", i + 1,
                              (int)lmrt_payloads.more.size(),
                              lmrt_payloads.more[i]);
    LOG(INFO) << StringPrintf("lmrt_log: %d entries in this packet",
                              lmrt_payloads.entry_count[i]);

    LOG(INFO) << StringPrintf("lmrt_log: tlv: %s", tlvs_str.c_str());
  }
}

/*******************************************************************************
**
** Function         lmrt_capture
**
** Description      record the last RF_SET_LISTEN_MODE_ROUTING_CMD
**
** Returns          None
**
*******************************************************************************/
void lmrt_capture(uint8_t* buf, uint8_t buf_size) {
  if (buf == nullptr || buf_size < 5) return;

  if (lmrt_payloads.more.size() > 0) {
    if (lmrt_payloads.more.back() == 0) {
      /* if the MORE setting of the last lmrt command is 0x00,
       * that means the data in lmrt_payloads are obsolete, empty it */
      debug_lmrt_init();
    }
  }

  /* push_back the last lmrt command to lmrt_payloads */
  lmrt_payloads.more.push_back(buf[3]);
  lmrt_payloads.entry_count.push_back(buf[4]);
  if (buf_size == 5) {
    lmrt_payloads.tlvs.push_back(std::vector<uint8_t>(0));
  } else {
    lmrt_payloads.tlvs.push_back(std::vector<uint8_t>(buf + 5, buf + buf_size));
  }
}

/*******************************************************************************
**
** Function         lmrt_update
**
** Description      Update the committed tlvs to committed_lmrt_tlvs
**
** Returns          None
**
*******************************************************************************/
void lmrt_update(void) {
  lmrt_log();
  std::vector<uint8_t> temp(0);

  /* combine all tlvs in lmrt_payloads.tlvs */
  for (auto tlv : lmrt_payloads.tlvs) {
    temp.insert(temp.end(), tlv.begin(), tlv.end());
  }

  committed_lmrt_tlvs.swap(temp);
}

/*******************************************************************************
**
** Function         lmrt_get_max_size
**
** Description      This function is used to get the max size of the routing
**                  table from cache
**
** Returns          Max Routing Table Size
**
*******************************************************************************/
int lmrt_get_max_size(void) { return nfc_cb.max_ce_table; }

/*******************************************************************************
**
** Function         lmrt_get_tlvs
**
** Description      This function is used to get the committed listen mode
**                  routing configuration command
**
** Returns          The committed listen mode routing configuration command
**
*******************************************************************************/
std::vector<uint8_t>* lmrt_get_tlvs() { return &committed_lmrt_tlvs; }
