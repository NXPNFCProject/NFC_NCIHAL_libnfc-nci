/*
 * Copyright 2010-2020, 2023-2025 NXP
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

#ifndef PHNFCTYPES_H
#define PHNFCTYPES_H

#include <stdint.h>

#define UNUSED_PROP(X) (void)(X);

typedef uint16_t NFCSTATUS; /* Return values */

/*
 * NFC Message structure contains message specific details like
 * message type, message specific data block details, etc.
 */
typedef struct phLibNfc_Message {
  uint32_t eMsgType; /* Type of the message to be posted*/
  void *pMsgData;    /* Pointer to message specific data block in case any*/
  uint32_t Size;     /* Size of the datablock*/
} phLibNfc_Message_t, *pphLibNfc_Message_t;

/*
 * Deferred call declaration.
 * This type of API is called from ClientApplication ( main thread) to notify
 * specific callback.
 */
typedef void (*pphLibNfc_DeferredCallback_t)(void *);

/*
 * Deferred parameter declaration.
 * This type of data is passed as parameter from ClientApplication (main thread)
 * to the
 * callback.
 */
typedef void *pphLibNfc_DeferredParameter_t;

/*
 * Deferred message specific info declaration.
 * This type of information is packed as message data when
 * PH_LIBNFC_DEFERREDCALL_MSG
 * type message is posted to message handler thread.
 */
typedef struct phLibNfc_DeferredCall {
  pphLibNfc_DeferredCallback_t pCallback;   /* pointer to Deferred callback */
  pphLibNfc_DeferredParameter_t pParameter; /* pointer to Deferred parameter */
} phLibNfc_DeferredCall_t;

/* PHNFCTYPES_H */
#endif
