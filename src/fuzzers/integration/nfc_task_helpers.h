#ifndef NFC_TASK_HELPERS_H_
#define NFC_TASK_HELPERS_H_

bool nfc_process_nci_messages();
void nfc_process_nfa_messages();
void DoAllTasks(bool do_timers);

#endif /* NFC_TASK_HELPERS_H_ */
