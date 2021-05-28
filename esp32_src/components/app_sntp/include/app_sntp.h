#ifndef SNTP_TASK_H
#define SNTP_TASK_H

void sntp_start(void);
bool sntp_wait_enabled(uint8_t timeout_sec);
size_t sntp_get_date(char *buf, size_t buflen);

#endif /* SNTP_TASK_H */
