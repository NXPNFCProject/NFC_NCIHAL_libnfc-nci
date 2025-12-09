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

#ifndef NXPNCIHALWRITER_H
#define NXPNCIHALWRITER_H

#include <atomic>
#include <cstdbool>
#include <pthread.h>

class WriterThread {
public:
  static WriterThread &getInstance();

  WriterThread(const WriterThread &) = delete;
  WriterThread &operator=(const WriterThread &) = delete;

  /******************************************************************************
   * Function:       Start()
   *
   * Description:    This method creates & initiates the worker thread for
   *                 handling Asynchronous write.
   *
   * Returns:        bool: True if the writer thread was successfully started
   *                 otherwise false.
   ******************************************************************************/
  bool Start();

  /******************************************************************************
   * Function:       Post()
   *
   * Description:    This method Posts message to the writer thread for async
   *                 write
   *
   * Returns:        bool: True if the writer thread was successfully post
   *                 message otherwise false.
   ******************************************************************************/
  bool Post(const uint8_t *data, uint16_t length);

  /******************************************************************************
   * Function:       Stop()
   *
   * Description:    This method stops the writer thread and clear the resources
   *
   * Returns:        bool: True if the writer thread was successfully stoped
   *                 otherwise false.
   ******************************************************************************/
  bool Stop();

private:
  WriterThread();
  ~WriterThread();

  static void *Run(void *arg);
  void Run();
  intptr_t writer_queue = 0;
  pthread_t writer_thread;
  volatile std::atomic<bool> thread_running;
};
#endif // NXPNCIHALWRITER_H
