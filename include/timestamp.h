#ifndef __TIMESTAMP_H__
#define __TIMESTAMP_H__

#ifdef __cplusplus
extern "C"
{
#endif

    unsigned long timestamp();
    unsigned long data2timestamp(const char* fmt_date, const char* date);
    void timestamp2date(unsigned long t, char* fmt_date, char* out_buf,int buf_len);
    unsigned long timestamp_today();
    unsigned long timestamp_yesterday();

#ifdef __cplusplus
}
#endif

#endif