#include "nfc_task_helpers.h"

#include <android-base/logging.h>

#include "gki.h"
#include "nfa_hci_int.h"
#include "nfa_sys.h"
#include "nfc_int.h"

void nfa_hci_check_api_requests(void);
void nfc_process_timer_evt(void);

bool nfc_process_nci_messages() {
  bool found_work = false;
  NFC_HDR* p_msg;
  bool free_buf;

  /* Process all incoming NCI messages */
  while ((p_msg = (NFC_HDR*)GKI_read_mbox(NFC_MBOX_ID)) != nullptr) {
    found_work = true;
    free_buf = true;

    /* Determine the input message type. */
    switch (p_msg->event & NFC_EVT_MASK) {
      case BT_EVT_TO_NFC_NCI:
        free_buf = nfc_ncif_process_event(p_msg);
        break;

      case BT_EVT_TO_START_TIMER:
        /* Start nfc_task 1-sec resolution timer */
        GKI_start_timer(NFC_TIMER_ID, GKI_SECS_TO_TICKS(1), true);
        break;

      case BT_EVT_TO_START_QUICK_TIMER:
        /* Quick-timer is required for LLCP */
        GKI_start_timer(NFC_QUICK_TIMER_ID,
                        ((GKI_SECS_TO_TICKS(1) / QUICK_TIMER_TICKS_PER_SEC)),
                        true);
        break;

      case BT_EVT_TO_NFC_MSGS:
        nfc_main_handle_hal_evt((tNFC_HAL_EVT_MSG*)p_msg);
        break;

      default:
        // Unknown message type
        CHECK(false);
        break;
    }

    if (free_buf) {
      GKI_freebuf(p_msg);
    }
  }
  return found_work;
}

void nfc_process_nfa_messages() {
  NFC_HDR* p_msg;
  while ((p_msg = (NFC_HDR*)GKI_read_mbox(NFA_MBOX_ID)) != nullptr) {
    nfa_sys_event(p_msg);
  }
}

void DoAllTasks(bool do_timers) {
start:
  bool found_work = false;
  while (nfc_process_nci_messages()) {
    found_work = true;
  }

  nfc_process_nfa_messages();

  if (do_timers) {
    nfc_process_timer_evt();
    nfc_process_quick_timer_evt();
  }

  nfa_hci_check_api_requests();

  nfa_sys_timer_update();

  if (nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE &&
      !GKI_queue_is_empty(&nfa_hci_cb.hci_api_q)) {
    found_work = true;
  }

  if (found_work) {
    goto start;
  }
}
