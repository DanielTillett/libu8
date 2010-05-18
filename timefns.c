/* -*- Mode: C; -*- */

/* Copyright (C) 2004-2010 beingmeta, inc.
   This file is part of the libu8 UTF-8 unicode library.

   This program comes with absolutely NO WARRANTY, including implied
   warranties of merchantability or fitness for any particular
   purpose.

    Use, modification, and redistribution of this program is permitted
    under any of the licenses found in the the 'licenses' directory 
    accompanying this distribution, including the GNU General Public License
    (GPL) Version 2 or the GNU Lesser General Public License.
*/

#include "libu8/libu8.h"

static char versionid[] MAYBE_UNUSED=
  "$Id$";

#include "libu8/u8streamio.h"
#include "libu8/u8printf.h"
#include "libu8/u8timefns.h"

#include <math.h>
/* We just include this for sscanf */
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#if HAVE_UUID_UUID_H
#include <uuid/uuid.h>
#endif

#if U8_THREADS_ENABLED
static u8_mutex timefns_lock;
#endif

/* Utility functions */

static MAYBE_UNUSED double getprecfactor(enum u8_timestamp_precision precision)
{
  switch (precision) {
  case u8_year: return 1000000000.0*3600*24*365;
  case u8_month: return 1000000000.0*3600*24*30;
  case u8_day: return 1000000000.0*3600*24;
  case u8_hour: return 1000000000.0*3600;
  case u8_minute: return 1000000000.0*60;
  case u8_second: return 1000000000.0;
  case u8_millisecond: return 1000000.0;
  case u8_microsecond: return 1000.0;
  case u8_nanosecond: return 1.0;
  default: return 1.0;}
}

static void copy_tm2xt(struct tm *tptr,struct U8_XTIME *xt)
{
  xt->u8_sec=tptr->tm_sec;
  xt->u8_min=tptr->tm_min;
  xt->u8_hour=tptr->tm_hour;
  xt->u8_mday=tptr->tm_mday;
  xt->u8_mon=tptr->tm_mon;
  xt->u8_year=tptr->tm_year+1900;
  xt->u8_wday=tptr->tm_wday;
  xt->u8_yday=tptr->tm_yday;
  xt->u8_wnum=(tptr->tm_yday/7);
}

static void copy_xt2tm(struct U8_XTIME *xt,struct tm *tptr)
{
  tptr->tm_sec=xt->u8_sec;
  tptr->tm_min=xt->u8_min;
  tptr->tm_hour=xt->u8_hour;
  tptr->tm_mday=xt->u8_mday;
  tptr->tm_mon=xt->u8_mon;
  tptr->tm_year=xt->u8_year-1900;
  tptr->tm_wday=xt->u8_wday;
  tptr->tm_yday=xt->u8_yday;
}

/* Finer grained time */

U8_EXPORT
/** Returns the number of milliseconds since the epoch.
    This returns a value with whatever accuracy it can get.
    @returns a long long counting milliseconds
*/
long long u8_millitime()
{
#if HAVE_GETTIMEOFDAY
  struct timeval now;
  if (gettimeofday(&now,NULL) < 0)
    return -1;
  else return now.tv_sec*1000+(now.tv_usec/1000);
#elif HAVE_FTIME
    struct timeb now;
#if WIN32
    /* In WIN32, ftime doesn't return an error value.
       ?? We should really do something respectable here.*/
    ftime(&now);
#else 
    if (ftime(&now) < 0) return -1;
    else return now.time*1000+now.millitm;
#endif
#else
    return ((int)time(NULL))*1000;
#endif
}

U8_EXPORT
/** Returns the number of microseconds since the epoch.
    This returns a value with whatever accuracy it can get.
    @returns a long long counting microseconds
*/
long long u8_microtime()
{
#if HAVE_GETTIMEOFDAY
  struct timeval now;
  if (gettimeofday(&now,NULL) < 0)
    return -1;
  else return now.tv_sec*1000000+now.tv_usec;
#elif HAVE_FTIME
  /* We're going to settle for millisecond precision here. */
  struct timeb now;
#if WIN32
  /* In WIN32, ftime doesn't return an error value.
     ?? We should really do something respectable here.*/
  ftime(&now);
#else 
  if (ftime(&now) < 0) return -1;
  else return now.time*1000000+now.millitm*1000;
#endif
#else
  return ((int)time(NULL))*1000000;
#endif
}

/* New core functionality */

unsigned int u8_precision_secs[12]=
  {1,3600*24*365,3600*24*30,3600*24,3600,60,1,1,1,1,1,1};

U8_EXPORT u8_xtime u8_init_xtime
  (struct U8_XTIME *xt,time_t tick,u8_tmprec prec,int nsecs,
   int tzoff,int dstoff)
{
  time_t offtick;
  unsigned int prec_secs;
  struct tm _tptr, *tptr=&_tptr;
  if (xt==NULL) xt=u8_malloc(sizeof(struct U8_XTIME));
  memset(xt,0,sizeof(struct U8_XTIME));
  if (tick<0) {
#if HAVE_GETTIMEOFDAY
    struct timeval tv; struct timezone tz;
    if (gettimeofday(&tv,&tz) < 0) {
      u8_graberr(errno,"u8_xlocaltime/gettimeofday",NULL);
      return NULL;}
    tick=tv.tv_sec;
    xt->u8_nsecs=tv.tv_usec*1000;
    if (prec<0) xt->u8_prec=u8_microsecond;
#elif HAVE_FTIME
    struct timeb tb;
    if (ftime(&tb)<0) {
      u8_graberr(errno,"u8_xlocaltime/ftime",NULL);
      return NULL;}
    tick=tb.time;
    xt->u8_nsecs=tb.millitm*1000000;
    if (prec<0) xt->u8_prec=u8_millisecond;
#else
    if ((tick=time(NULL))<0) {
      u8_graberr(errno,"u8_xlocaltime/time",NULL);
      return NULL;}
    if (prec<0) xt->u8_prec=u8_second;
#endif
  }

  /* Offset the tick to get a "fake" gmtime */
  offtick=tick+tzoff+dstoff;

  /* Adjust for the precision, rounding to the middle of the range */
  prec_secs=u8_precision_secs[prec];
  offtick=offtick-(offtick%prec_secs)+prec_secs/2;

  /* Get the broken down representation for the "fake" time. */
#if HAVE_GMTIME_R
  gmtime_r(&offtick,&_tptr);
#else
  u8_lock_mutex(&timefns_lock);
  tptr=gmtime(&offtick);
#endif
  copy_tm2xt(tptr,xt);
#if (!(HAVE_GMTIME_R))
  u8_unlock_mutex(&timefns_lock);
#endif
  /* Set the real time and the offset */
  xt->u8_tick=tick; xt->u8_tzoff=tzoff; xt->u8_dstoff=dstoff;
  /* Initialize the precision */
  if (prec<0) prec=u8_second;
  xt->u8_prec=prec;
  /* Don't have redundant nanoseconds */
  if (xt->u8_prec<=u8_second) xt->u8_nsecs=0;
  return xt;
}

U8_EXPORT u8_xtime u8_local_xtime
  (struct U8_XTIME *xt,time_t tick,u8_tmprec prec,int nsecs)
{
  unsigned int prec_secs;
  struct tm _tptr, *tptr=&_tptr;
  if (xt==NULL) xt=u8_malloc(sizeof(struct U8_XTIME));
  memset(xt,0,sizeof(struct U8_XTIME));
  if (tick<0) {
#if HAVE_GETTIMEOFDAY
    struct timeval tv; struct timezone tz;
    if (gettimeofday(&tv,&tz) < 0) {
      u8_graberr(errno,"u8_xlocaltime/gettimeofday",NULL);
      return NULL;}
    tick=tv.tv_sec;
    xt->u8_nsecs=tv.tv_usec*1000;
    if ((prec<0) || (prec>u8_microsecond))
      prec=u8_microsecond;
#elif HAVE_FTIME
    struct timeb tb;
    if (ftime(&tb)<0) {
      u8_graberr(errno,"u8_xlocaltime/ftime",NULL);
      return NULL;}
    tick=tb.time;
    xt->u8_nsecs=tb.millitm*1000000;
    if ((prec<0) || (prec>u8_millisecond))
      prec=u8_millisecond;
#else
    if ((tick=time(NULL))<0) {
      u8_graberr(errno,"u8_xlocaltime/time",NULL);
      return NULL;}
    if ((prec<0) || (prec>u8_second))
      prec=u8_second;
#endif
  }

  /* Adjust for the precision */
  prec_secs=u8_precision_secs[prec];
  tick=tick-(tick%prec_secs)+prec_secs/2;

  /* Get the broken down representation for the "fake" time. */
#if HAVE_LOCALTIME_R
  localtime_r(&tick,&_tptr);
#else
  u8_lock_mutex(&timefns_lock);
  tptr=localtime(&tick);
#endif
  copy_tm2xt(tptr,xt);
#if (!(HAVE_LOCALTIME_R))
  u8_unlock_mutex(&timefns_lock);
#endif
  /* Set the real time and the offset */
  xt->u8_tick=tick;
#if HAVE_TM_GMTOFF
  if (tptr->tm_isdst) {
    xt->u8_tzoff=tptr->tm_gmtoff-3600;
    xt->u8_dstoff=3600;}
  else xt->u8_tzoff=tptr->tm_gmtoff;
#elif ((HAVE_GETTIMEOFDAY) || (HAVE_FTIME))
  if (daylight) {
    xt->u8_tzoff=-timezone-3600;
    xt->u8_dstoff=3600;}
  else xt->u8_tzoff=-timezone;
#else
  xt->u8_tzoff=0;
#endif
  /* Initialize the precision */
  if (prec<0) prec=u8_second;
  xt->u8_prec=prec;
  /* Don't have redundant nanoseconds */
  if (xt->u8_prec<=u8_second) xt->u8_nsecs=0;
  return xt;
}

U8_EXPORT
/* u8_mktime
     Arguments: a pointer to a tm struct and a time_t value
     Returns: the time_t value or -1 if it failed
*/
time_t u8_mktime(struct U8_XTIME *xt)
{
  time_t moment; struct tm tptr;
  memset(&tptr,0,sizeof(struct tm));
  copy_xt2tm(xt,&tptr);
  /* This tells it to figure it out. */
  moment=mktime(&tptr);
  /* The moment is what time it would be if the specified
     time were in the current timezone and appropriate
     daylight savings regimen for that time of year. */
  if (tptr.tm_isdst) moment=moment-3600;
  if ((xt->u8_dstoff==0) &&
      (tptr.tm_isdst) &&
      (!(xt->u8_forcedst))) {
    /* Our argument says its not DST, but it would be locally,
       so we assume that it is and adjust tzoff and dstoff
       appropriately. If we *know* the dstoff value, the caller
       should set the u8_forcedst field. */
    xt->u8_dstoff=3600; xt->u8_tzoff=xt->u8_tzoff-3600;}
  if (moment<0) {}
  else if ((xt->u8_tzoff+xt->u8_dstoff)!=tptr.tm_gmtoff) 
    /* This means that the current timezone assumed by mktime
       is different from the timezone specified for our U8_XTIME
       object. So we adjust the tick to compensate for where we really
       are.  */
    moment=moment+((tptr.tm_gmtoff)-(xt->u8_tzoff+xt->u8_dstoff));
  xt->u8_tick=moment;
  return moment;
}

/* Initializing xtime structures */

U8_EXPORT
/* u8_localtime
     Arguments: a pointer to a tm struct and a time_t value
     Returns: the time_t value or -1 if it failed
*/
time_t u8_localtime_x(struct U8_XTIME *xt,time_t tick,int nsecs)
{
  struct tm _now, *now=&_now;
#if HAVE_LOCALTIME_R
  localtime_r(&tick,&_now);
#else
  u8_lock_mutex(&timefns_lock);
  now=localtime(&tick);
  if (now == NULL) {
    u8_unlock_mutex(&timefns_lock);
    return -1;}
#endif
  copy_tm2xt(now,xt);
#if HAVE_TM_GMTOFF
  xt->u8_tzoff=now->tm_gmtoff;
#endif
#if (!(HAVE_LOCALTIME_R))
  u8_unlock_mutex(&timefns_lock);
#endif
  xt->u8_tick=tick;
  if (nsecs<0) {
    xt->u8_nsecs=0;
    xt->u8_prec=u8_second;}
  else {xt->u8_nsecs=nsecs; xt->u8_prec=u8_nanosecond;}
  return tick;
}
U8_EXPORT
/* u8_localtime
     Arguments: a pointer to a tm struct and a time_t value
     Returns: the time_t value or -1 if it failed
*/
time_t u8_localtime(struct U8_XTIME *xt,time_t tick)
{
  return u8_localtime_x(xt,tick,-1);
}

U8_EXPORT
/* u8_set_xtime_precision
     Arguments: a pointer to an xtime struct and an integer
     Returns: void
*/
void u8_set_xtime_precision(struct U8_XTIME *xt,u8_tmprec prec)
{
  xt->u8_prec=prec;
}

U8_EXPORT
/* u8_xtime_plus
     Arguments: a pointer to an xtime struct and an integer
     Returns: void
*/
void u8_xtime_plus(struct U8_XTIME *xt,double delta)
{
  int sign=(delta>=0.0);
  double absval=((delta<0.0) ? (-delta) : (delta));
  double secs=floor(absval);
  double ndelta=(delta-absval)*1000000000.0;
  u8_tmprec precision=xt->u8_prec;
  time_t tick=xt->u8_tick; unsigned int nsecs=xt->u8_nsecs;
  int tzoff=xt->u8_tzoff, dstoff=xt->u8_dstoff;
  tick=((sign) ? (tick+secs) : (tick-secs));
  if ((sign) && (nsecs+ndelta>1000000000)) {
    tick++; nsecs=(nsecs+ndelta)-1000000000;}
  if ((!(sign)) && (xt->u8_nsecs-ndelta<0)) {
    tick--; nsecs=(nsecs-ndelta)+1000000000;}
  u8_init_xtime(xt,tick,precision,nsecs,tzoff,dstoff);
  /* Reset the precision */
  xt->u8_prec=precision;
}

U8_EXPORT
/* u8_xtime_diff
     Arguments: pointers to two xtime structs
     Returns: void
*/
double u8_xtime_diff(struct U8_XTIME *xt,struct U8_XTIME *yt)
{
  double xsecs=((double)(xt->u8_tick))+(0.000000001*xt->u8_nsecs);
  double ysecs=((double)(yt->u8_tick))+(0.000000001*yt->u8_nsecs);
  double totaldiff=xsecs-ysecs;
#if 0 /* This doesn't seem to work, but the idea is to only return a result
	 as precise as is justified. */
  enum u8_timestamp_precision result_precision=
    ((xt->u8_prec<yt->u8_prec) ? (xt->u8_prec) : (yt->u8_prec));
  double precfactor=getprecfactor(result_precision);
  return trunc(precfactor*totaldiff)/precfactor;
#endif
  return totaldiff;
}

U8_EXPORT
/* u8_xtime_to_tptr
     Arguments: pointers to an xtime struct and a tm struct
     Returns: void
*/
time_t u8_xtime_to_tptr(struct U8_XTIME *xt,struct tm *tm)
{
  
  memset(tm,0,sizeof(struct tm));
  copy_xt2tm(xt,tm);
  tm->tm_gmtoff=xt->u8_tzoff;
  if (xt->u8_dstoff) tm->tm_isdst=1;
  return mktime(tm);
}

U8_EXPORT
/* u8_now:
     Arguments: a pointer to an extended time pointer
     Returns: a time_t or -1 if it fails for some reason

  This will try and get the finest precision time it can.
*/
time_t u8_now(struct U8_XTIME *xtp)
{
  if (xtp) {
    u8_xtime result=u8_local_xtime(xtp,-1,u8_femtosecond,0);
    if (result) return result->u8_tick;
    else return -1;}
  else {
    time_t result=time(NULL);
    if (result<0) {
      u8_graberr(errno,"u8_now",NULL); errno=0;}
    return result;}
}

/* Time and strings */

struct TZENTRY {char *name; int tzoff; int dstoff;};
static struct TZENTRY tzones[]= {
  {"Z",0,0},
  {"GMT",0,0},
  {"UT",0,0},
  {"UTC",0,0},  
  {"EST",-5*3600,0},
  {"EDT",-5*3600,3600},
  {"CST",-6*3600,0},
  {"CDT",-6*3600,3600},
  {"MST",-7*3600,0},
  {"MDT",-7*3600,0},
  {"PST",-8*3600,0},
  {"PDT",-8*3600,3600},
  {"CET",1*3600,0},
  {"EET",2*3600,0},
  {NULL,0,0}};

static int lookup_tzname(char *string,int dflt)
{
  struct TZENTRY *zones=tzones;
  while ((*zones).name)
    if (strcasecmp(string,(*zones).name) == 0)
      return (*zones).tzoff;
    else zones++;
  return dflt;
}

U8_EXPORT
/* u8_parse_tzspec:
     Arguments: a string and a default offset
     Returns: an offset from UTC

This uses a built in table but should really use operating system
facilities if they were even remotely standardized.
*/
int u8_parse_tzspec(u8_string s,int dflt)
{
  int hours=0, mins=0, secs=0, sign=1;
  int dhours=0, dmins=0, dsecs=0, dsign=1;
  char *offstart=strchr(s,'+'), *dstart;
  if (offstart == NULL) {
    offstart=strchr(s,'-');
    if (offstart) sign=-1;
    else return lookup_tzname(s,dflt);}
  sscanf(offstart+1,"%d:%d:%d",&hours,&mins,&secs);
  return sign*(hours*3600+mins*60+secs);
}

U8_EXPORT
/* u8_iso8601_to_xtime:
     Arguments: a string and a pointer to a timestamp structure
     Returns: -1 on error, the time as a time_t otherwise

This takes an iso8601 string and fills out an extended time pointer which
includes possible timezone and precision information.
*/
time_t u8_iso8601_to_xtime(u8_string s,struct U8_XTIME *xtp)
{
  char *tzstart;
  int pos[]={-1,4,7,10,13,16,19,20}, nsecs=0, n_elts;
  if (strchr(s,'/')) return (time_t) -1;
  n_elts=sscanf(s,"%04u-%02hhu-%02hhuT%02hhu:%02hhu:%02hhu.%u",
		&xtp->u8_year,&xtp->u8_mon,
		&xtp->u8_mday,&xtp->u8_hour,
		&xtp->u8_min,&xtp->u8_sec,
		&nsecs);
  /* Give up if you can't parse anything */
  if (n_elts == 0) return (time_t) -1;
  /* Adjust month */
  xtp->u8_mon--;
  /* Set precision */
  xtp->u8_prec=n_elts;
  if (n_elts <= 6) xtp->u8_nsecs=0;
  if (n_elts == 7) {
    char *start=s+pos[n_elts], *scan=start; int zeros=0;
    while (*scan == '0') {zeros++; scan++;}
    while (isdigit(*scan)) scan++;
    xtp->u8_nsecs=nsecs*(9-zeros);
    xtp->u8_prec=xtp->u8_prec+((scan-start)/3);
    tzstart=scan;}
  else tzstart=s+pos[n_elts];
  /* Handle our own little extension of IS9601 to separate
     out DST and TZ offsets */
  if ((tzstart=="+") || (tzstart=="-")) {
    int dsign=1;
    char *dststart=strchr(tzstart+1,'+');
    if (dststart==NULL) {
      dststart=strchr(tzstart+1,'-'); dsign=-1;}
    if (dststart) {
      int hr=0, min=0, sec=0;
      sscanf(dststart+1,"%d:%d:%d",&hr,&min,&sec);
      xtp->u8_dstoff=dsign*(3600*hr+60*min+sec);
      xtp->u8_forcedst=1;}}
  xtp->u8_tzoff=u8_parse_tzspec(tzstart,xtp->u8_tzoff);
  xtp->u8_tick=u8_mktime(xtp); xtp->u8_nsecs=0;
  return xtp->u8_tick;
}

U8_EXPORT
/* u8_xtime_to_iso8601:
     Arguments: a timestamp and a pointer to a string stream
     Returns: -1 on error, the time as a time_t otherwise

This takes an iso8601 string and fills out an extended time pointer which
includes possible timezone and precision information.
*/
void u8_xtime_to_iso8601(u8_output ss,struct U8_XTIME *xt)
{
  char buf[128], tzbuf[128];
  switch (xt->u8_prec) {
  case u8_year:
    sprintf(buf,"%04d",xt->u8_year); break;
  case u8_month:
    sprintf(buf,"%04d-%02d",xt->u8_year,xt->u8_mon+1); break;
  case u8_day:
    sprintf(buf,"%04d-%02d-%02d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday); break;
  case u8_hour:
    sprintf(buf,"%04d-%02d-%02dT%02d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour);
    break;
  case u8_minute:
    sprintf(buf,"%04d-%02d-%02dT%02d:%02d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour,xt->u8_min);
    break;
  case u8_second:
    sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour,xt->u8_min,xt->u8_sec);
    break;
  case u8_millisecond:
    sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d.%03d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour,xt->u8_min,xt->u8_sec,
	    xt->u8_nsecs/1000000); break;
  case u8_microsecond:
    sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d.%03d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour,xt->u8_min,xt->u8_sec,
	    xt->u8_nsecs/1000); break;
  case u8_nanosecond:
    sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d.%09d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour,xt->u8_min,xt->u8_sec,
	    xt->u8_nsecs); break;
  case u8_picosecond:
    sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d.%012d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour,xt->u8_min,xt->u8_sec,
	    xt->u8_nsecs); break;
  case u8_femtosecond:
    sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d.%015d",
	    xt->u8_year,xt->u8_mon+1,xt->u8_mday,
	    xt->u8_hour,xt->u8_min,xt->u8_sec,
	    xt->u8_nsecs); break;}
  if ((xt->u8_tzoff) ||  (xt->u8_dstoff)) {
    int off=xt->u8_tzoff+xt->u8_dstoff;
    char *sign=((off<0) ? "-" : "+");
    int tzoff=((off<0) ? (-((off))) : (off));
    int hours=tzoff/3600,
      minutes=(tzoff%3600)/60,
      seconds=tzoff%3600-minutes*60;
    if (seconds)
      sprintf(tzbuf,"%s%d:%02d:%02d",sign,hours,minutes,seconds);
    else sprintf(tzbuf,"%s%d:%02d",sign,hours,minutes);}
  else strcpy(tzbuf,"UTC");
  if (xt->u8_prec > u8_day)
    u8_printf(ss,"%s%s",buf,tzbuf);
  else u8_printf(ss,"%s",buf);
}

static u8_string month_names[12]=
  {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static u8_string dow_names[7]=
  {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static int getmonthnum(u8_string s)
{
  int i=0;
  while (i<12)
    if (strncasecmp(s,month_names[i],3)==0)
      return i;
    else i++;
  return -1;
}

U8_EXPORT
/* u8_rfc822_to_xtime:
     Arguments: a string and a pointer to a timestamp structure
     Returns: -1 on error, the time as a time_t otherwise

This takes an iso8601 string and fills out an extended time pointer which
includes possible timezone and precision information.
*/
time_t u8_rfc822_to_xtime(u8_string s,struct U8_XTIME *xtp)
{
  char tzspec[128], dow[128], mon[128];
  int n_elts;
  if (strchr(s,'/')) return (time_t) -1;
  if (isdigit(*s)) 
    n_elts=sscanf(s,"%hhd %s %d %hhd:%hhd:%hhd %s",
		  &xtp->u8_mday,(char *)&mon,
		  &xtp->u8_year,
		  &xtp->u8_hour,
		  &xtp->u8_min,&xtp->u8_sec,
		  (char *)tzspec);
  else {
    n_elts=sscanf(s,"%s %hhd %s %d %hhd:%hhd:%hhd %s",
		  (char *)&dow,&xtp->u8_mday,(char *)&mon,
		  &xtp->u8_year,
		  &xtp->u8_hour,
		  &xtp->u8_min,&xtp->u8_sec,
		  (char *)tzspec);
    xtp->u8_mon=getmonthnum(mon);}
  /* Give up if you can't parse anything */
  if (n_elts == 0) return (time_t) -1;
  /* Set precision */
  xtp->u8_prec=u8_second;
  xtp->u8_nsecs=0;
  xtp->u8_tzoff=u8_parse_tzspec(tzspec,xtp->u8_tzoff);
  xtp->u8_tick=u8_mktime(xtp); xtp->u8_nsecs=0;
  return xtp->u8_tick;
}

U8_EXPORT
/* u8_xtime_to_rfc822:
     Arguments: a timestamp and a pointer to a string stream
     Returns: -1 on error, the time as a time_t otherwise

This takes an iso8601 string and fills out an extended time pointer which
includes possible timezone and precision information.
*/
void u8_xtime_to_rfc822(u8_output ss,struct U8_XTIME *xtp)
{
  struct U8_XTIME asgmt;
  char buf[128];
  u8_init_xtime(&asgmt,xtp->u8_tick,xtp->u8_prec,xtp->u8_nsecs,0,0);
  sprintf(buf,"%s, %d %s %04d %02d:%02d:%02d",
	  dow_names[asgmt.u8_wday],
	  asgmt.u8_mday,
	  month_names[asgmt.u8_mon],
	  (asgmt.u8_year),
	  asgmt.u8_hour,asgmt.u8_min,asgmt.u8_sec);
  u8_printf(ss,"%s GMT",buf);
}

/* printf handling for time related values */

static u8_string time_printf_handler
  (u8_output s,char *cmd,u8_string buf,int bufsz,va_list *args)
{
  struct U8_XTIME xt;
  if (strchr(cmd,'*'))
    /* Uses the current time, doesn't consume an argument. */
    if (strchr(cmd+1,'G'))
      u8_init_xtime(&xt,-1,u8_femtosecond,0,0,0);
    else u8_local_xtime(&xt,-1,u8_femtosecond,0);
  else if (strchr(cmd,'X'))
    /* The argument is an U8_XTIME pointer. */
    if (strchr(cmd,'G')) {
      /* Convert the result to GMT/UTC */
      struct U8_XTIME *xtarg=va_arg(*args,u8_xtime);
      memcpy(&xt,xtarg,sizeof(struct U8_XTIME));}
    else {
      /* Use the time intrinsic to the XTIME structure. */
      struct U8_XTIME *xtarg=va_arg(*args,u8_xtime);
      memcpy(&xt,xtarg,sizeof(struct U8_XTIME));}
  /* Otherwise, the argument is time_t value. */
  else if (strchr(cmd,'G')) 
    /* Use it as GMT. */
    u8_init_xtime(&xt,va_arg(*args,time_t),u8_second,0,0,0);
  else /* Display it as local time */
    u8_local_xtime(&xt,va_arg(*args,time_t),u8_second,0);
  if (strchr(cmd,'i')) {
    /* With this argument, specify the precision to be used. */
    u8_tmprec precision=u8_second;
    if (strchr(cmd,'Y')) precision=u8_year;
    else if (strchr(cmd,'D')) precision=u8_day;
    else if (strstr(cmd,"HM")) precision=u8_minute;
    else if (strstr(cmd,"MS")) precision=u8_millisecond;
    else if (strchr(cmd,'H')) precision=u8_hour;
    else if (strchr(cmd,'S')) precision=u8_second;
    else if (strchr(cmd,'U')) precision=u8_microsecond;
    else if (strchr(cmd,'M')) precision=u8_month;
    else if (strchr(cmd,'N')) precision=u8_nanosecond;
    xt.u8_prec=precision;
    u8_xtime_to_iso8601(s,&xt);
    return NULL;}
  else if (strchr(cmd,'l')) {
    /* With 'l' output the date together with the time. */
    struct tm tmp; copy_xt2tm(&xt,&tmp);
    strftime(buf,bufsz,"%d%b%Y@%H:%M:%S%z",&tmp);
    return buf;}
  else {
    /* With no flags, just output the time. */
    struct tm tmp; copy_xt2tm(&xt,&tmp);
    strftime(buf,bufsz,"%H:%M:%S",&tmp);
    return buf;}
}

/* UUID generation and related functions */

#if HAVE_GETUUID
U8_EXPORT u8_uuid *u8_getuuid()
{
  uuid_t tmp;
  u8_uuid *data=u8_malloc(16*sizeof(unsigned char));
  uuid_generate(tmp);
  memcpy(data,tmp,16);
  return data;
}

U8_EXPORT u8_string u8_getuuidstring()
{
  uuid_t tmp; u8_string str=u8_malloc(37*sizeof(unsigned char));
  uuid_generate(tmp);
  uuid_unparse(tmp,str);
  return str;
}

U8_EXPORT u8_uuid *u8_parseuuid(u8_string s)
{
  uuid_t tmp;
  u8_uuid *data=u8_malloc(16*sizeof(unsigned char));
  uuid_parse((char *)s,tmp);
  memcpy(data,tmp,16);
  return data;
}
#else
#endif

/* Initialization functions */

U8_EXPORT void u8_init_timefns_c()
{
  u8_printf_handlers['t']=time_printf_handler;
#if U8_THREADS_ENABLED
  u8_init_mutex(&timefns_lock);
#endif
}
