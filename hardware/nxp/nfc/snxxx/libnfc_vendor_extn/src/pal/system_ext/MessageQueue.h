/*
 * Copyright 2010-2014, 2023, 2025 NXP
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
 * DAL independent message queue implementation for Android
 */

#ifndef NFC_MESSAGEQUEUE_H
#define NFC_MESSAGEQUEUE_H

#include <linux/ipc.h>
#include <phNfcTypes.h>
#include <sys/types.h>

intptr_t MessageQueue_get(key_t key, int msgflg);
void MessageQueue_postSemaphore(intptr_t msqid);
void MessageQueue_destroy(intptr_t msqid);
intptr_t MessageQueue_send(intptr_t msqid, phLibNfc_Message_t *msg, int msgflg);
int MessageQueue_receive(intptr_t msqid, phLibNfc_Message_t *msg, long msgtyp,
                         int msgflg);

#endif /*  NFC_MESSAGEQUEUE_H  */
