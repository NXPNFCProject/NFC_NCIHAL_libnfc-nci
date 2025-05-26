/*
 *
 *  Copyright 2021-2025 NXP
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
#define LOG_TAG "PalThreadMutex"

#include "PalThreadMutex.h"
#include "phNxpLog.h"

/*******************************************************************************
**
** Function:    PalThreadMutex::PalThreadMutex()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
PalThreadMutex::PalThreadMutex() {
  pthread_mutexattr_t mutexAttr;

  pthread_mutexattr_init(&mutexAttr);
  if (pthread_mutex_init(&mMutex, &mutexAttr)) {
    NXPLOG_EXTNS_D(LOG_TAG, "init mutex success");
  } else {
    NXPLOG_EXTNS_E(LOG_TAG, "fail to init mutex");
  }
  pthread_mutexattr_destroy(&mutexAttr);
}

/*******************************************************************************
**
** Function:    PalThreadMutex::~PalThreadMutex()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
PalThreadMutex::~PalThreadMutex() { pthread_mutex_destroy(&mMutex); }

/*******************************************************************************
**
** Function:    PalThreadMutex::lock()
**
** Description: lock kthe mutex
**
** Returns:     none
**
*******************************************************************************/
void PalThreadMutex::lock() { pthread_mutex_lock(&mMutex); }

/*******************************************************************************
**
** Function:    PalThreadMutex::unblock()
**
** Description: unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
void PalThreadMutex::unlock() { pthread_mutex_unlock(&mMutex); }

/*******************************************************************************
**
** Function:    PalThreadCondVar::PalThreadCondVar()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
PalThreadCondVar::PalThreadCondVar() {
  pthread_condattr_t CondAttr;

  pthread_condattr_init(&CondAttr);
  pthread_condattr_setclock(&CondAttr, CLOCK_MONOTONIC);
  pthread_cond_init(&mCondVar, &CondAttr);

  pthread_condattr_destroy(&CondAttr);
}

/*******************************************************************************
**
** Function:    PalThreadCondVar::~PalThreadCondVar()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
PalThreadCondVar::~PalThreadCondVar() { pthread_cond_destroy(&mCondVar); }

/*******************************************************************************
**
** Function:    PalThreadCondVar::timedWait()
**
** Description: wait on the mCondVar or till timeout happens
**
** Returns:     none
**
*******************************************************************************/
void PalThreadCondVar::timedWait(struct timespec *time) {
  pthread_cond_timedwait(&mCondVar, *this, time);
}

/*******************************************************************************
**
** Function:    PalThreadCondVar::timedWait()
**
** Description: wait on the mCondVar or till timeout happens
**
** Returns:     none
**
*******************************************************************************/
void PalThreadCondVar::timedWait(uint8_t sec) {
  struct timespec timeout_spec;
  clock_gettime(CLOCK_MONOTONIC, &timeout_spec);
  timeout_spec.tv_sec += sec;
  pthread_cond_timedwait(&mCondVar, *this, &timeout_spec);
}

/*******************************************************************************
**
** Function:    PalThreadCondVar::signal()
**
** Description: signal the mCondVar
**
** Returns:     none
**
*******************************************************************************/
void PalThreadCondVar::signal() {
  PalAutoThreadMutex a(*this);
  pthread_cond_signal(&mCondVar);
}

/*******************************************************************************
**
** Function:    PalAutoThreadMutex::PalAutoThreadMutex()
**
** Description: class constructor, automatically lock the mutex
**
** Returns:     none
**
*******************************************************************************/
PalAutoThreadMutex::PalAutoThreadMutex(PalThreadMutex &m) : mm(m) { mm.lock(); }

/*******************************************************************************
**
** Function:    PalAutoThreadMutex::~PalAutoThreadMutex()
**
** Description: class destructor, automatically unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
PalAutoThreadMutex::~PalAutoThreadMutex() { mm.unlock(); }
