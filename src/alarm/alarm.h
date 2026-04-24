#ifndef ALARM_H
#define ALARM_H

void clock_init(int hour, int min, int sec);
void clock_update(void);
void alarm_set(int hour, int min, int sec);
void alarm_clear(void);
void clock_get(int *hour, int *min, int *sec);

// can use clock_get(*hr, *min, *sec) for current time logic
// alarm_set(hr, min, sec) trigger alarm at time
// clock_init(hr, min, sec) to set current time
// alarm_clear() clear alarm (rn led auto clear 10 sec)

#endif