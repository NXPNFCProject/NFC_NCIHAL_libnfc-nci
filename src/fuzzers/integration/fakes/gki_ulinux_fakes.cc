#include "android-base/logging.h"
#include "gki.h"
#include "gki_int.h"

tGKI_CB gki_cb;

void* GKI_os_malloc(uint32_t size) { return malloc(size); }

void GKI_os_free(void* p) {
  if (p) {
    free(p);
  }
}

void GKI_exception(uint16_t, std::string s) { LOG(ERROR) << s; }

// We fuzz in a single thread so locks and scheduling are not implemented

void GKI_disable(void) {}
void GKI_enable(void) {}

void GKI_exit_task(unsigned char) { CHECK(false); }

uint8_t GKI_get_taskid() { return NFC_TASK; }
uint32_t GKI_get_os_tick_count() { return 0; }

void GKI_sched_unlock() {}
void GKI_sched_lock() {}

uint8_t GKI_send_event(unsigned char, unsigned short) { return 0; }

void GKI_shiftup(unsigned char*, unsigned char*, unsigned int) { CHECK(false); }

uint16_t GKI_wait(unsigned short, unsigned int) { abort(); }
