/*
 * Copyright (C) 2015 NXP Semiconductors
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

#define LOG_TAG "NxpNfcNciHal"

#include <errno.h>
#include <hardware/hardware.h>
#include <hardware/nfc.h>
#include <log/log.h>
#include <phNxpNciHal_Adaptation.h>
#include <string.h>
#include <stdlib.h>
#include "hal_nxpnfc.h"
/*****************************************************************************
 * NXP NCI HAL Function implementations.
 *****************************************************************************/

/*******************************************************************************
**
** Function         hal_open
**
** Description      It opens and initialzes the physical connection with NFCC.
**
** Returns          0 if successful
**
*******************************************************************************/
static int hal_open(__attribute__((unused)) const struct nfc_nci_device* p_dev,
                    nfc_stack_callback_t p_hal_cback,
                    nfc_stack_data_callback_t* p_hal_data_callback) {
  return phNxpNciHal_open(p_hal_cback, p_hal_data_callback);
}

/*******************************************************************************
**
** Function         hal_write
**
** Description      Write the data to NFCC.
**
** Returns          Number of bytes successfully written to NFCC.
**
*******************************************************************************/
static int hal_write(__attribute__((unused)) const struct nfc_nci_device* p_dev,
                      uint16_t data_len, const uint8_t* p_data) {
  return phNxpNciHal_write(data_len, p_data);
}

/*******************************************************************************
**
** Function         hal_ioctl
**
** Description      Invoke ioctl to  to NFCC driver.
**
** Returns          status code of ioctl.
**
*******************************************************************************/
static int hal_ioctl(const nxpnfc_nci_device_t* p_dev, long arg,
                     void* p_data) {
  int retval = 0;
  nxpnfc_nci_device_t* dev = (nxpnfc_nci_device_t*)p_dev;

  retval = phNxpNciHal_ioctl(arg, p_data);
  return retval;
}

/*******************************************************************************
**
** Function         hal_core_initialized
**
** Description      Notify NFCC after successful initialization of NFCC.
**                  All proprietary settings can be done here.
**
** Returns          0 if successful
**
*******************************************************************************/
static int hal_core_initialized(__attribute__((unused))
                                const struct nfc_nci_device* p_dev,
                                uint8_t* p_core_init_rsp_params) {
  return retval = phNxpNciHal_core_initialized(p_core_init_rsp_params);
}

/*******************************************************************************
**
** Function         hal_pre_discover
**
** Description      Notify NFCC before start discovery.
**
** Returns          0 if successful
**
*******************************************************************************/
static int hal_pre_discover(__attribute__((unused)) const struct nfc_nci_device* p_dev) {
  int retval = 0;
  return phNxpNciHal_pre_discover();
}

/*******************************************************************************
**
** Function         hal_close
**
** Description      Close the NFCC interface and free all resources.
**
** Returns          0 if successful
**
*******************************************************************************/
static int hal_close(__attribute__((unused)) const
                        struct nfc_nci_device* p_dev) {
  return phNxpNciHal_close();
}

/*******************************************************************************
**
** Function         hal_control_granted
**
** Description      Notify NFCC that control is granted to HAL.
**
** Returns          0 if successful
**
*******************************************************************************/
static int hal_control_granted(__attribute__((unused)) const
                                struct nfc_nci_device* p_dev) {
  return phNxpNciHal_control_granted();
}

/*******************************************************************************
**
** Function         hal_power_cycle
**
** Description      Notify power cycling has performed.
**
** Returns          0 if successful
**
*******************************************************************************/
static int hal_power_cycle(__attribute__((unused)) const
                            struct nfc_nci_device* p_dev) {
  return phNxpNciHal_power_cycle();
}

/*******************************************************************************
**
** Function         hal_get_fw_dwnld_flag
**
** Description      Notify FW download request.
**
** Returns          true if successful otherwise false.
**
*******************************************************************************/
static int hal_get_fw_dwnld_flag(__attribute__((unused)) const
                                 nxpnfc_nci_device_t* p_dev,
                                 uint8_t* fwDnldRequest) {
  return phNxpNciHal_getFWDownloadFlag(fwDnldRequest);
}

/*************************************
 * Generic device handling.
 *************************************/

/*******************************************************************************
**
** Function         nfc_close
**
** Description      Close the nfc device instance.
**
** Returns          0 if successful
**
*******************************************************************************/
static int nfc_close(hw_device_t* dev) {
  int retval = 0;
  free(dev);
  return retval;
}

/*******************************************************************************
**
** Function         nfc_open
**
** Description      Open the nfc device instance.
**
** Returns          0 if successful
**
*******************************************************************************/
static int nfc_open(const hw_module_t* module, const char* name,
                    hw_device_t** device) {
  ALOGD("%s: enter; name=%s", __func__, name);
  int retval = 0; /* 0 is ok; -1 is error */
  nxpnfc_nci_device_t* dev = NULL;
  if (strcmp(name, NFC_NCI_CONTROLLER) == 0) {
    dev = calloc(1, sizeof(nxpnfc_nci_device_t));
    if (dev == NULL) {
      retval = -EINVAL;
    } else {
      /* Common hw_device_t fields */
      dev->nci_device.common.tag = HARDWARE_DEVICE_TAG;
      dev->nci_device.common.version =
          0x00010000; /* [31:16] major, [15:0] minor */
      dev->nci_device.common.module = (struct hw_module_t*)module;
      dev->nci_device.common.close = nfc_close;

      /* NCI HAL method pointers */
      dev->nci_device.open = hal_open;
      dev->nci_device.write = hal_write;
      dev->nci_device.core_initialized = hal_core_initialized;
      dev->nci_device.pre_discover = hal_pre_discover;
      dev->nci_device.close = hal_close;
      dev->nci_device.control_granted = hal_control_granted;
      dev->nci_device.power_cycle = hal_power_cycle;
      dev->ioctl = hal_ioctl;
      dev->check_fw_dwnld_flag = hal_get_fw_dwnld_flag;
      *device = (hw_device_t*)dev;
    }
  } else {
    retval = -EINVAL;
  }

  ALOGD("%s: exit %d", __func__, retval);
  return retval;
}

/* Android hardware module definition */
static struct hw_module_methods_t nfc_module_methods = {
    .open = nfc_open,
};

/* NFC module definition */
struct nfc_nci_module_t HAL_MODULE_INFO_SYM = {
    .common =
        {
         .tag = HARDWARE_MODULE_TAG,
         .module_api_version = 0x0100, /* [15:8] major, [7:0] minor (1.0) */
         .hal_api_version = 0x00,      /* 0 is only valid value */
         .id = NFC_NCI_HARDWARE_MODULE_ID,
         .name = "NXP PN54X NFC NCI HW HAL",
         .author = "NXP Semiconductors",
         .methods = &nfc_module_methods,
        },
};
