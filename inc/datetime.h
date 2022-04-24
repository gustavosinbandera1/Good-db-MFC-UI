// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< DATEIME.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:     11-Aug-2000  K.A. Knizhnik  * / [] \ *
//                          Last update: 11-Aug-2000  K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Date-time field type
//-------------------------------------------------------------------*--------*

#ifndef __DATETIME_H__
#define __DATETIME_H__

#include "goods.h"
#include <time.h>

BEGIN_GOODS_NAMESPACE

class dbDateTime {
    int4 timestamp;
  public:
    bool operator == (dbDateTime const& dt) {
        return timestamp == dt.timestamp;
    }
    bool operator != (dbDateTime const& dt) {
        return timestamp != dt.timestamp;
    }
    bool operator > (dbDateTime const& dt) {
        return timestamp > dt.timestamp;
    }
    bool operator >= (dbDateTime const& dt) {
        return timestamp >= dt.timestamp;
    }
    bool operator < (dbDateTime const& dt) {
        return timestamp < dt.timestamp;
    }
    bool operator <= (dbDateTime const& dt) {
        return timestamp <= dt.timestamp;
    }
    int operator - (dbDateTime const& dt) {
        return timestamp - dt.timestamp;
    }
     int operator + (dbDateTime const& dt) {
        return timestamp + dt.timestamp;
    }
    static dbDateTime current() {
        return dbDateTime(time(NULL));
    }
    dbDateTime(time_t t) {
        timestamp = t;
    }
    dbDateTime() {
        timestamp = -1;
    }
    bool isValid() {
        return timestamp != -1;
    }

    time_t asTime_t() { return timestamp; }

    void clear() { timestamp = -1; }

    dbDateTime(int year, int month, int day,
               int hour=0, int min=0, int sec = 0)
    {
        struct tm t;
        t.tm_year = year > 1900 ? year - 1900 : year;
        t.tm_mon = month-1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_sec = sec;
        t.tm_isdst = -1;
        timestamp = mktime(&t);
    }
    dbDateTime(int hour, int min) {
        timestamp = (hour*60+min)*60;
    }

#if HAVE_LOCALTIME_R
    int year() {
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_year + 1900;
    }
    int month() { // 1..12
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_mon + 1;
    }
    int day() { // 1..31
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_mday;
    }
    int dayOfYear() { // 1..366
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_yday+1;
    }
    int dayOfWeek() { // 1..7
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_wday+1;
    }
    int hour() { // 0..24
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_hour;
    }
    int minute() { // 0..59
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_min;
    }
    int second() { // 0..59
        struct tm t;
        return localtime_r((time_t*)&timestamp, &t)->tm_min;
    }
    char* asString(char* buf, int buf_size, char const* format = "%c") const {
        struct tm t;
        strftime(buf, buf_size, format, localtime_r((time_t*)&timestamp, &t));
        return buf;
    }
    static dbDateTime currentDate() {
        struct tm t;
        time_t curr = time(NULL);
        localtime_r(&curr, &t);;
        t.tm_hour = 0;
        t.tm_min = 0;
        t.tm_sec = 0;
        return dbDateTime(mktime(&t));
    }
#else
    int year() {
        return localtime((time_t*)&timestamp)->tm_year + 1900;
    }
    int month() { // 1..12
        return localtime((time_t*)&timestamp)->tm_mon + 1;
    }
    int day() { // 1..31
        return localtime((time_t*)&timestamp)->tm_mday;
    }
    int dayOfYear() { // 1..366
        return localtime((time_t*)&timestamp)->tm_yday+1;
    }
    int dayOfWeek() { // 1..7
        return localtime((time_t*)&timestamp)->tm_wday+1;
    }
    int hour() { // 0..24
        return localtime((time_t*)&timestamp)->tm_hour;
    }
    int minute() { // 0..59
        return localtime((time_t*)&timestamp)->tm_min;
    }
    int second() { // 0..59
        return localtime((time_t*)&timestamp)->tm_min;
    }
    char* asString(char* buf, int buf_size, char const* format = "%c") const {
        strftime(buf, buf_size, format, localtime((time_t*)&timestamp));
        return buf;
    }
    static dbDateTime currentDate() {
        time_t curr = time(NULL);
        struct tm* tp = localtime(&curr);;
        tp->tm_hour = 0;
        tp->tm_min = 0;
        tp->tm_sec = 0;
        return dbDateTime(mktime(tp));
    }
#endif

    friend field_descriptor& describe_field(dbDateTime& dt);

    field_descriptor& describe_components() { return FIELD(timestamp); }
};

inline field_descriptor& describe_field(dbDateTime& dt) { 
    return dt.describe_components();
}


class dbDate {
    int4 jday;
  public:
    bool operator == (dbDate const& dt) {
        return jday == dt.jday;
    }
    bool operator != (dbDate const& dt) {
        return jday != dt.jday;
    }
    bool operator > (dbDate const& dt) {
        return jday > dt.jday;
    }
    bool operator >= (dbDate const& dt) {
        return jday >= dt.jday;
    }
    bool operator < (dbDate const& dt) {
        return jday < dt.jday;
    }
    bool operator <= (dbDate const& dt) {
        return jday <= dt.jday;
    }
    int operator - (dbDate const& dt) {
        return jday - dt.jday;
    }
    int operator + (int days) {
        return jday + days;
    }
    static dbDate current() {
        time_t now = time(NULL);
        struct tm* tp;
#if HAVE_LOCALTIME_R
        struct tm t;
        tp = localtime_r(&now, &t);
#else
        tp = localtime(&now);
#endif
        return dbDate(tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday);
    }

    dbDate() {
        jday = -1;
    }
    bool isValid() {
        return jday != -1;
    }

    unsigned JulianDay() { return jday; }

    void clear() { jday = -1; }

    dbDate(int year, int month, int day)  {
    /*
      Convert Gregorian calendar date to the corresponding Julian day number
      j.  Algorithm 199 from Communications of the ACM, Volume 6, No. 8,
      (Aug. 1963), p. 444.  Gregorian calendar started on Sep. 14, 1752.
      This function not valid before that.
      */
        nat4 c, ya;
        if (month > 2)
            month -= 3;
        else {
            month += 9;
            year--;
        } /* else */
        c = year / 100;
        ya = year - 100*c;
        jday = ((146097*c)>>2) + ((1461*ya)>>2) + (153*month + 2)/5 + day + 1721119;
    } /* jday */

    void MDY(int& year, int& month, int& day) const {
    /*
      Convert a Julian day number to its corresponding Gregorian calendar
      date.  Algorithm 199 from Communications of the ACM, Volume 6, No. 8,
      (Aug. 1963), p. 444.  Gregorian calendar started on Sep. 14, 1752.
      This function not valid before that.
     */
        nat4 j = jday - 1721119;
        int m, d, y;
        y = (((j<<2) - 1) / 146097);
        j = (j<<2) - 1 - 146097*y;
        d = (j>>2);
        j = ((d<<2) + 3) / 1461;
        d = ((d<<2) + 3 - 1461*j);
        d = (d + 4)>>2;
        m = (5*d - 3)/153;
        d = 5*d - 3 - 153*m;
        d = (d + 5)/5;
        y = (100*y + j);
        if (m < 10) {
                m += 3;
        } else {
                m -= 9;
                y++;
        } /* else */
        month = m;
        day = d;
        year = y;
    } /* mdy */

    int day() {
        int month, day, year;
        MDY(year, month, day);
        return day;
    }

    int month() {
        int month, day, year;
        MDY(year, month, day);
        return month;
    }

    int year() {
        int month, day, year;
        MDY(year, month, day);
        return year;
    }

    int dayOfWeek() {
        return (jday % 7) + 1;
    }

    char* asString(char* buf, char const* format = "%d-%M-%Y") const {
        static const char* dayName[] = { "Mon", "Tue", "Wen", "Thu", "Fri", "Sat", "Sun" };
        static const char* monthName[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                                           "Aug", "Sep", "Oct", "Nov", "Dec" };
        int month, day, year;
        MDY(year, month, day);
        char ch, *dst = buf;
        while ((ch = *format++) != '\0') {
            if (ch == '%') {
                ch = *format++;
                switch (ch) {
                  case 'd': dst += sprintf(dst, "%02u", day ); continue;
                  case 'D': dst += sprintf(dst, "%s",   dayName[jday % 7]); continue;
                  case 'm': dst += sprintf(dst, "%02u", month); continue;
                  case 'M': dst += sprintf(dst, "%s",   monthName[month - 1]); continue;
                  case 'y': dst += sprintf(dst, "%02u", year - 1900); continue;
                  case 'Y': dst += sprintf(dst, "%04u", year); continue;
                  default: *dst++ = ch;
                }
            } else { 
                *dst++ = ch;
            }
        }
        *dst = '\0';
        return buf;
    }

    friend field_descriptor& describe_field(dbDate& dt);

    field_descriptor& describe_components() { return FIELD(jday); }
};

inline field_descriptor& describe_field(dbDate& dt) { 
    return dt.describe_components();
}

END_GOODS_NAMESPACE

#endif







