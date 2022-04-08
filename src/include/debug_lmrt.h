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

#ifndef _DEBUG_LMRT_
#define _DEBUG_LMRT_

#include <stdint.h>

#include <string>
#include <vector>

#include "nfc_int.h"

/* The type definition of a group of RF_SET_LISTEN_MODE_ROUTING_CMD(s) */
typedef struct lmrt_payload_t {
  std::vector<uint8_t> more;
  std::vector<uint8_t> entry_count;
  std::vector<std::vector<uint8_t>> tlvs;
} __attribute__((__packed__)) lmrt_payload_t;

/*******************************************************************************
**
** Function         debug_lmrt_init
**
** Description      initialize the lmrt_payloads
**
** Returns          None
**
*******************************************************************************/
void debug_lmrt_init(void);

/*******************************************************************************
**
** Function         lmrt_log
**
** Description      print the listen mode routing configuration for debug use
**
** Returns          None
**
*******************************************************************************/
void lmrt_log(void);

/*******************************************************************************
**
** Function         lmrt_capture
**
** Description      record the last RF_SET_LISTEN_MODE_ROUTING_CMD
**
** Returns          None
**
*******************************************************************************/
void lmrt_capture(uint8_t* buf, uint8_t buf_size);

/*******************************************************************************
**
** Function         lmrt_update
**
** Description      Update the committed tlvs to committed_lmrt_tlvs
**
** Returns          None
**
*******************************************************************************/
void lmrt_update(void);

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
int lmrt_get_max_size(void);

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
std::vector<uint8_t>* lmrt_get_tlvs();

#endif /* _DEBUG_LMRT_ */
