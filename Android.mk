# function to find all *.cpp files under a directory
define all-cpp-files-under
$(patsubst ./%,%, \
  $(shell cd $(LOCAL_PATH) ; \
          find $(1) -name "*.cpp" -and -not -name ".*") \
 )
endef


LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
NFA := src/nfa
NFC := src/nfc
HAL := src/hal
UDRV := src/udrv

D_CFLAGS := -DANDROID -DBUILDCFG=1

#NXP PN547 Enable
D_CFLAGS += -DNXP_EXTNS=TRUE
#variables for NFC_NXP_CHIP_TYPE
PN547C2 := 1
PN548C2 := 2
PN551C2 := 3
NQ110 := $PN547C2
NQ120 := $PN547C2
NQ210 := $PN548C2
NQ220 := $PN548C2

ifeq ($(PN547C2),1)
D_CFLAGS += -DPN547C2=1
endif
ifeq ($(PN548C2),2)
D_CFLAGS += -DPN548C2=2
endif
ifeq ($(PN551C2),3)
D_CFLAGS += -DPN551C2=3
endif

#### Select the JCOP OS Version ####
JCOP_VER_3_0 := 1
JCOP_VER_3_1_1 := 2
JCOP_VER_3_1_2 := 3
JCOP_VER_3_2 := 4

LOCAL_CFLAGS += -DJCOP_VER_3_0=$(JCOP_VER_3_0)
LOCAL_CFLAGS += -DJCOP_VER_3_1_1=$(JCOP_VER_3_1_1)
LOCAL_CFLAGS += -DJCOP_VER_3_1_2=$(JCOP_VER_3_1_2)
LOCAL_CFLAGS += -DJCOP_VER_3_2=$(JCOP_VER_3_2)

NFC_NXP_ESE:= FALSE
ifeq ($(NFC_NXP_ESE),TRUE)
LOCAL_CFLAGS += -DNFC_NXP_ESE=TRUE
LOCAL_CFLAGS += -DNFC_NXP_ESE_VER=$(JCOP_VER_3_1_2)
else
LOCAL_CFLAGS += -DNFC_NXP_ESE=FALSE
endif

#### Select the CHIP ####
D_CFLAGS += -DNFC_NXP_CHIP_TYPE=PN548C2

#Gemalto SE support
D_CFLAGS += -DGEMALTO_SE_SUPPORT
D_CFLAGS += -DNXP_UICC_ENABLE
D_CFLAGS += -DNFC_POWER_MANAGEMENT=FALSE
######################################
# Build shared library system/lib/libnfc-nci.so for stack code.

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libnfc-nci
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libhardware_legacy libcutils liblog libdl libhardware
LOCAL_CFLAGS += $(D_CFLAGS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src/include \
    $(LOCAL_PATH)/src/gki/ulinux \
    $(LOCAL_PATH)/src/gki/common \
    $(LOCAL_PATH)/$(NFA)/include \
    $(LOCAL_PATH)/$(NFA)/int \
    $(LOCAL_PATH)/$(NFC)/include \
    $(LOCAL_PATH)/$(NFC)/int \
    $(LOCAL_PATH)/src/hal/include \
    $(LOCAL_PATH)/src/hal/int \
    $(LOCAL_PATH)/$(HALIMPL)/include
LOCAL_SRC_FILES := \
    $(call all-c-files-under, $(NFA)/ce $(NFA)/dm $(NFA)/ee) \
    $(call all-c-files-under, $(NFA)/hci $(NFA)/int $(NFA)/p2p $(NFA)/rw $(NFA)/sys) \
    $(call all-c-files-under, $(NFC)/int $(NFC)/llcp $(NFC)/nci $(NFC)/ndef $(NFC)/nfc $(NFC)/tags) \
    $(call all-c-files-under, src/adaptation) \
    $(call all-cpp-files-under, src/adaptation) \
    $(call all-c-files-under, src/gki) \
    src/nfca_version.c
include $(BUILD_SHARED_LIBRARY)


######################################
include $(call all-makefiles-under,$(LOCAL_PATH))
