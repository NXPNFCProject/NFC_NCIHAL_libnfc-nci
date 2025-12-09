/*
 * Copyright 2025 NXP
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

#include "WriterThread.h"
#include "NfcExtensionApi.h"
#include <MessageQueue.h>
#include <android-base/stringprintf.h>
#include <log/log.h>
#include <phNfcNciConstants.h>
#include <phNxpConfig.h>
#include <phNxpLog.h>
#include <vector>

#define NCI_HAL_TML_WRITE_MSG 0x417

WriterThread::WriterThread() : writer_thread(0), thread_running(false) {
  writer_queue = 0;
}

WriterThread::~WriterThread() { Stop(); }

WriterThread &WriterThread::getInstance() {
  static WriterThread instance;
  return instance;
}

bool WriterThread::Start() {
  /* The thread_running.load() and thread_running.store() methods are
     part of the C++11 std::atomic class. These methods are used to
     perform atomic operations on variables, which ensures that the
     operations are thread-safe without needing explicit locks or mutexes */
  if (!thread_running.load()) {
    thread_running.store(true);
    writer_queue = MessageQueue_get(0, 0600);
    if (writer_queue == 0) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s:Failed to create writer queue", __func__);
      return false;
    }
    int val = pthread_create(&writer_thread, NULL, WriterThread::Run, this);
    if (val != 0) {
      thread_running.store(false);
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:pthread_create failed",
                     __func__);
      return false;
    }
  }
  return true;
}

bool WriterThread::Post(const uint8_t *data, uint16_t length) {
  phLibNfc_Message_t msg;
  if (!data || length == 0 || length > NCI_MAX_DATA_LEN) {
    NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:Invalid input buffer",
                   __func__);
    return false;
  }
  msg.eMsgType = NCI_HAL_TML_WRITE_MSG;
  msg.Size = length;
  memcpy(msg.data, data, length);
  if (MessageQueue_send(writer_queue, &msg, 0) != 0) {
    return false;
  }
  return true;
}

bool WriterThread::Stop() {
  if (thread_running.load()) {
    thread_running.store(false);
    // Unblock writer thread
    MessageQueue_postSemaphore(writer_queue);
    if (pthread_join(writer_thread, (void **)NULL) != 0) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s:pthread_join failed",
                     __func__);
      MessageQueue_destroy(writer_queue);
      writer_queue = 0;
      return false;
    }
    writer_thread = 0;
    MessageQueue_destroy(writer_queue);
    writer_queue = 0;
  }
  return true;
}

void *WriterThread::Run(void *arg) {
  WriterThread *worker = static_cast<WriterThread *>(arg);
  worker->Run();
  return nullptr;
}

void WriterThread::Run() {
  phLibNfc_Message_t msg;
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "WriterThread started");

  while (thread_running.load()) {
    memset(&msg, 0x00, sizeof(phLibNfc_Message_t));
    if (MessageQueue_receive(writer_queue, &msg, 0, 0) == -1) {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "WriterThread received bad message");
      continue;
    }
    if (!thread_running.load()) {
      break;
    }
    switch (msg.eMsgType) {
    case NCI_HAL_TML_WRITE_MSG: {
      NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN,
                     "%s: Received NCI_HAL_TML_WRITE_MSG", __func__);
      std::vector<uint8_t> buffer(msg.Size);
      memcpy(buffer.data(), msg.data, msg.Size);
      if (getNfcVendorExtnCb() != nullptr) {
        if (getNfcVendorExtnCb()->aidlHal != nullptr) {
          int ret;
          getNfcVendorExtnCb()->aidlHal->write(buffer, &ret);
        } else if (getNfcVendorExtnCb()->hidlHal != nullptr) {
          ::android::hardware::nfc::V1_0::NfcData data;
          data.setToExternal(buffer.data(), msg.Size);
          getNfcVendorExtnCb()->hidlHal->write(data);
        } else {
          NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No HAL to Write!",
                         __func__);
        }
      } else {
        NXPLOG_EXTNS_E(NXPLOG_ITEM_NXP_GEN_EXTN, "%s No Vendor Extension CB!",
                       __func__);
      }
      break;
    }
    }
  }
  NXPLOG_EXTNS_I(NXPLOG_ITEM_NXP_GEN_EXTN, "WriterThread stopped");
  return;
}
