/*
 * Copyright (C) 2012 The Android Open Source Project
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
/******************************************************************************
 *
 *  The original Work has been changed by NXP.
 *
 *  Copyright 2022,2025 NXP
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

/*
 *  Asynchronous interval timer.
 */
#define LOG_TAG "PalIntervalTimer"

#include "phNxpLog.h"
#include <PalIntervalTimer.h>
#include <errno.h>

PalIntervalTimer::PalIntervalTimer() {
  mCb = NULL;
}

bool PalIntervalTimer::set(int ms, void *ptr, TIMER_FUNC cb, timer_t* timerId) {
  if (cb == nullptr || timerId == nullptr) {
    return false;
  }

  if (!create(ptr, cb, timerId)) {
    return false;
  }
  if (cb != mCb) {
    kill(timerId);
    if (!create(ptr, cb, timerId)) return false;
  }

  int stat = 0;
  struct itimerspec ts;
  ts.it_value.tv_sec = ms / 1000;
  ts.it_value.tv_nsec = (ms % 1000) * 1000000;

  ts.it_interval.tv_sec = 0;
  ts.it_interval.tv_nsec = 0;

  stat = timer_settime(*timerId, 0, &ts, 0);
  if (stat == -1) {
    NXPLOG_EXTNS_D(LOG_TAG, "%s fail to set timer, errno : %x", __func__,
                   errno);
  }
  return stat == 0;
}

PalIntervalTimer::~PalIntervalTimer() {
}

void PalIntervalTimer::kill(timer_t* timerId) {
  if (*timerId == 0) return;
  if (timer_delete(*timerId) != 0) {
    NXPLOG_EXTNS_E(LOG_TAG, "fail delete timer");
  }

  *timerId = 0;

  if (mCb != NULL)
    mCb = NULL;
}

bool PalIntervalTimer::create(void* ptr, TIMER_FUNC cb, timer_t* timerId) {
  if (cb == nullptr || timerId == nullptr) {
    return false;
  }
  struct sigevent se;
  int stat = 0;

  /*
   * Set the sigevent structure to cause the signal to be
   * delivered by creating a new thread.
   */
  se.sigev_notify = SIGEV_THREAD;
  // se.sigev_value.sival_ptr = &mTimerId;
  se.sigev_value.sival_ptr = ptr;
  se.sigev_notify_function = cb;
  se.sigev_notify_attributes = NULL;
  se.sigev_signo = 0;
  mCb = cb;
  stat = timer_create(CLOCK_MONOTONIC, &se, timerId);
  if (stat == -1)
    NXPLOG_EXTNS_D(LOG_TAG, "fail create timer");
  return stat == 0;
}
