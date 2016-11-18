# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


HAL_SUFFIX := $(TARGET_DEVICE)
ifeq ($(TARGET_DEVICE),crespo)
	HAL_SUFFIX := herring
endif

#Enable NXP Specific
D_CFLAGS += -DNXP_EXTNS=TRUE

#variables for NFC_NXP_CHIP_TYPE
PN547C2 := 1
PN548C2 := 2
PN551   := 3
PN553   := 4

NQ110 := $PN547C2
NQ120 := $PN547C2
NQ210 := $PN548C2
NQ220 := $PN548C2

#Enable HCE-F specific
D_CFLAGS += -DNXP_NFCC_HCE_F=TRUE

#NXP PN547 Enable
ifeq ($(PN547C2),1)
D_CFLAGS += -DPN547C2=1
endif
ifeq ($(PN548C2),2)
D_CFLAGS += -DPN548C2=2
endif
ifeq ($(PN551),3)
D_CFLAGS += -DPN551=3
endif
ifeq ($(PN553),4)
D_CFLAGS += -DPN553=4
endif

#### Select the CHIP ####
NXP_CHIP_TYPE := $(PN553)

ifeq ($(NXP_CHIP_TYPE),$(PN547C2))
D_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN547C2
else ifeq ($(NXP_CHIP_TYPE),$(PN548C2))
D_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN548C2
else ifeq ($(NXP_CHIP_TYPE),$(PN551))
D_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN551
else ifeq ($(NXP_CHIP_TYPE),$(PN553))
D_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN553
endif

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
ifeq ($(NXP_CHIP_TYPE),$(PN547C2))
LOCAL_MODULE := nfc_nci_pn547.grouper
else ifeq ($(NXP_CHIP_TYPE),$(PN548C2))
LOCAL_MODULE := nfc_nci.pn54x.default
else ifeq ($(NXP_CHIP_TYPE),$(PN551))
LOCAL_MODULE := nfc_nci.pn54x.default
else ifeq ($(NXP_CHIP_TYPE),$(PN553))
LOCAL_MODULE := nfc_nci.pn54x.default
endif
ifeq (true,$(TARGET_IS_64_BIT))
LOCAL_MULTILIB := 64
else
LOCAL_MULTILIB := 32
endif
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := $(call all-subdir-c-files)  $(call all-subdir-cpp-files)
LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware_legacy libdl

LOCAL_CFLAGS := $(D_CFLAGS)
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/common \
    $(LOCAL_PATH)/dnld \
    $(LOCAL_PATH)/hal \
    $(LOCAL_PATH)/log \
    $(LOCAL_PATH)/tml \
    $(LOCAL_PATH)/configs \
    $(LOCAL_PATH)/self-test

LOCAL_CFLAGS += -DANDROID \
        -DNXP_HW_SELF_TEST
LOCAL_CFLAGS += -DNFC_NXP_HFO_SETTINGS=FALSE
NFC_NXP_ESE:= TRUE
ifeq ($(NFC_NXP_ESE),TRUE)
D_CFLAGS += -DNFC_NXP_ESE=TRUE
ifeq ($(NXP_CHIP_TYPE),$(PN553))
D_CFLAGS += -DJCOP_WA_ENABLE=FALSE
else
D_CFLAGS += -DJCOP_WA_ENABLE=TRUE
endif
else
D_CFLAGS += -DNFC_NXP_ESE=FALSE
endif
LOCAL_CFLAGS += $(D_CFLAGS)
#LOCAL_CFLAGS += -DFELICA_CLT_ENABLE
#-DNXP_PN547C1_DOWNLOAD
include $(BUILD_SHARED_LIBRARY)
    
