#include "android-base/logging.h"
#include "gki.h"

// Timers are handled explicitly by the fuzzer
void GKI_start_timer(unsigned char, int, bool) {}
void GKI_stop_timer(uint8_t) {}

uint32_t GKI_get_remaining_ticks(TIMER_LIST_Q*, TIMER_LIST_ENT*) {
  CHECK(false);
  return 0;
}

void GKI_init_timer_list(TIMER_LIST_Q* p_timer_listq) {
  new (p_timer_listq) TIMER_LIST_Q;
}

void GKI_add_to_timer_list(TIMER_LIST_Q* p_timer_listq, TIMER_LIST_ENT* p_tle) {
  if (!p_tle || !p_timer_listq || p_tle->ticks < 0) {
    return;
  }

  p_tle->in_use = true;

  auto it = p_timer_listq->begin();
  while (it != p_timer_listq->end()) {
    TIMER_LIST_ENT* p_successor = *it;

    // Reached successor
    if (p_tle->ticks <= p_successor->ticks) {
      p_successor->ticks -= p_tle->ticks;
      break;
    }

    // Account for predecessor ticks
    if (p_successor->ticks > 0) {
      p_tle->ticks -= p_successor->ticks;
    }

    it++;
  }

  p_timer_listq->insert(it, p_tle);
}

void GKI_remove_from_timer_list(TIMER_LIST_Q* p_timer_listq,
                                TIMER_LIST_ENT* p_tle) {
  if (!p_tle || !p_tle->in_use) {
    return;
  }

  auto it = std::find(p_timer_listq->begin(), p_timer_listq->end(), p_tle);
  if (it == p_timer_listq->end()) {
    return;
  }

  auto successor_it = p_timer_listq->erase(it);

  // Add remaining ticks to subsequent timer
  if (successor_it != p_timer_listq->end()) {
    (*successor_it)->ticks += p_tle->ticks;
  }

  p_tle->ticks = 0;
  p_tle->in_use = false;
}

uint16_t GKI_update_timer_list(TIMER_LIST_Q* p_timer_listq,
                               int32_t num_units_since_last_update) {
  uint16_t num_time_out = 0;

  for (auto it : *p_timer_listq) {
    if (it->ticks <= 0) {
      num_time_out++;
      continue;
    }

    if (num_units_since_last_update <= 0) {
      break;
    }

    int32_t temp_ticks = it->ticks;
    it->ticks -= num_units_since_last_update;

    // Check for timeout
    if (it->ticks <= 0) {
      it->ticks = 0;
      num_time_out++;
    }

    // Decrement ticks to process
    num_units_since_last_update -= temp_ticks;
  }

  return num_time_out;
}

bool GKI_timer_list_empty(TIMER_LIST_Q* p_timer_listq) {
  return p_timer_listq->empty();
}

TIMER_LIST_ENT* GKI_timer_list_first(TIMER_LIST_Q* p_timer_listq) {
  return *p_timer_listq->begin();
}

uint32_t g_tick_count = 0;
uint32_t GKI_get_tick_count() { return g_tick_count++; }
