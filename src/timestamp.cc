#include "timestamp.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

// 定义一个跨平台通用的日期时间格式字符串，用于strftime函数格式化输出
// 格式为：年-月-日 时:分:秒
#if defined(_WIN32)
#define DATE_FORMAT "%Y-%m-%d %H:%M:%S"
#else
#define DATE_FORMAT "%F %T"
#endif

// 获取当前时间戳（从1970年1月1日00:00:00 UTC到当前时间的秒数）
unsigned long timestamp() {
    time_t now = time(NULL);
    return (unsigned long)now;
}

// 将指定格式的日期字符串转换为时间戳
unsigned long data2timestamp(const char* fmt_date, const char* date) {
    struct tm tm_info = {0};
    if (strptime(date, fmt_date, &tm_info) == NULL) {
        return 0;  // 转换失败返回0
    }
    time_t t = mktime(&tm_info);
    return (unsigned long)t;
}

// 将时间戳转换为指定格式（包含年月日时分秒）的日期字符串并存入输出缓冲区
void timestamp2date(unsigned long t, char* fmt_date, char* out_buf, int buf_len) {
    time_t time_value = (time_t)t;
    struct tm* tm_info = localtime(&time_value);
    if (tm_info == NULL) {
        strncpy(out_buf, "", buf_len);
        return;
    }
    // 使用预定义的跨平台日期时间格式字符串进行格式化
    strftime(out_buf, buf_len, DATE_FORMAT, tm_info);
}

// 获取当天的起始时间戳（即当天00:00:00的时间戳）
unsigned long timestamp_today() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    tm_info->tm_hour = 0;
    tm_info->tm_min = 0;
    tm_info->tm_sec = 0;
    return (unsigned long)mktime(tm_info);
}

// 获取昨天的起始时间戳（即昨天00:00:00的时间戳）
unsigned long timestamp_yesterday() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    tm_info->tm_hour = 0;
    tm_info->tm_min = 0;
    tm_info->tm_sec = 0;
    time_t yesterday = now - 24 * 60 * 60;  // 减去一天的秒数
    struct tm* yester_tm_info = localtime(&yesterday);
    return (unsigned long)mktime(yester_tm_info);
}