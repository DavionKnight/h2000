/*
 * @file		rtc.c
 * @author	Tianzhy <tianzy@huahuan.com>
 * @date		2013-8-17
 * @modif   zhangjj<zhangjj@huahuan.com>
 */
#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>
#include <linux/rtc.h>
#include <time.h>
#include <string.h>

#include "api.h"

#define RTCDEV	"/dev/rtc0"
/*********************************
 *get rtc time
 *********************************/
int rtc_get_time(int *piYear, int *piMonth,  int *piDay, 
        int *piHour, int *piMinute, int *piSecond)
{
    int fd = -1;
    int ret = 0;
    struct rtc_time  sRtcTime;  

    fd=open(RTCDEV, O_RDONLY);

    if(fd  >= 0)
    {
        ret = ioctl(fd, RTC_RD_TIME, &sRtcTime);
        if(ret<0)
        {
            cdebug("ioctl error\n");
            close(fd);
            return(-1);
        }
        if(piSecond)  *piSecond = sRtcTime.tm_sec;
        if(piMinute)  *piMinute = sRtcTime.tm_min;
        if(piHour)    *piHour   = sRtcTime.tm_hour;
        if(piDay )    *piDay    = sRtcTime.tm_mday;
        if(piDay )    *piMonth  = sRtcTime.tm_mon + 1;
        if(piDay )    *piYear   = sRtcTime.tm_year+1900;	

        close(fd);
        return(0);
    }
    else
    {
        cdebug("open /dev/rtc  error\n");
        close(fd);
        return(-1);
    }
}

/*********************************
 *set rtc time
 *设置系统日历，如2013 9 4 17 21 10
 *rtc_set_time(2013,9,4,17,21 10)
 *
 ************************************************/
int rtc_set_time(int  iYear, int  iMonth,  int  iDay,
        int  iHour, int  iMinute, int  iSecond) 
{
    int fd = -1;
    int ret = 0;

    fd=open(RTCDEV, O_WRONLY);

    if(fd >= 0)
    {
        struct rtc_time  sRtcTime;  
        sRtcTime.tm_sec   = iSecond;	
        sRtcTime.tm_min   = iMinute;
        sRtcTime.tm_hour  = iHour;
        sRtcTime.tm_mday  = iDay;
        sRtcTime.tm_mon   = iMonth - 1;
        sRtcTime.tm_year  = iYear-1900;
        sRtcTime.tm_wday  = 0;
        sRtcTime.tm_yday  = 0;
        sRtcTime.tm_isdst = 0;

        ret = ioctl(fd, RTC_SET_TIME, &sRtcTime);

        if(ret<0)
        {
            cdebug("ioctl error\n");
            close(fd);
            return(-1);
        }
        close(fd);
        return(0);
    }else {
        cdebug("open /dev/rtc  error\n");
        close(fd);
        return(-1);
    }
}

//recover time from RTC
int rtc_recover_systime(){
    struct tm sTmTime;
    struct timeval sSetTime;

    int           iValues[6] = {0};
    rtc_get_time(&iValues[0], &iValues[1],  &iValues[2], &iValues[3], &iValues[4], &iValues[5]);

    memset(&sTmTime, 0, sizeof(struct tm));
    sTmTime.tm_year = iValues[0] - 1900; 
    sTmTime.tm_mon  = iValues[1] - 1; 
    sTmTime.tm_mday = iValues[2]; 
    sTmTime.tm_hour = iValues[3]; 
    sTmTime.tm_min  = iValues[4]; 
    sTmTime.tm_sec  = iValues[5]; 

    sSetTime.tv_sec  = mktime(&sTmTime);
    sSetTime.tv_usec = 0;
    settimeofday(&sSetTime, NULL);
	return 0;
}


#if 0
   int main(void)
   {    //just for test
   int time[6];
   int i, result;
//   rtc_recover_systime();
//	rtc_set_time(2015,10,5,18,21,10);

   result = rtc_get_time(&time[0],&time[1], &time[2], &time[3], &time[4], &time[5]);

   printf("\n");
   for(i=0; i<6; i++)
   {
   printf("%d \t", time[i]);
   }
   printf("\n");

//   rtc_set_time(2012,9,4,17,21,10);
   return 0;
   }
#endif
