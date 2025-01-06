#ifndef __MY_TIMER_LIST_H__
#define __MY_TIMER_LIST_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

struct timer;

struct timer* schedule_repeat(void(*on_timer)(void* udata),
									void* udata, 
									int after_sec,
									int repeat_count,
									int repeat_msec);

void cancel_timer(struct timer* t);

struct timer* schedule_once(void(*on_timer)(void* udata),
											void* udata, 
												int after_sec);
void* get_timer_udata(struct timer* t);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !__TIMER_LIST_H__