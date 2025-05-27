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

#ifndef __PALINTERVALTIMER_H__
#define __PALINTERVALTIMER_H__
/*
 *  Asynchronous interval timer.
 */

#include <time.h>

class PalIntervalTimer {
public:
  typedef void (*TIMER_FUNC)(union sigval);
  PalIntervalTimer();
  ~PalIntervalTimer();
  bool set(int ms, void *ptr, TIMER_FUNC cb, timer_t* timerId);
  void kill(timer_t timerId);
  bool create(void *ptr, TIMER_FUNC, timer_t* timerId);

private:
  TIMER_FUNC mCb;
};
#endif // __PALINTERVALTIMER_H__
