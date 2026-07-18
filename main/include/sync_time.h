#ifndef SYNC_TIME_H_
#define SYNC_TIME_H_

void sync_sntp_time(void);
void init_sntp(void);
bool time_is_synced(void);

#endif // SYNC_TIME_H_