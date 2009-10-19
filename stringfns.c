/* -*- Mode: C; -*- */

/* Copyright (C) 2004-2009 beingmeta, inc.
   This file is part of the libu8 UTF-8 unicode library.

   This program comes with absolutely NO WARRANTY, including implied
   warranties of merchantability or fitness for any particular
   purpose.

    Use, modification, and redistribution of this program is permitted
    under any of the licenses found in the the 'licenses' directory 
    accompanying this distribution, including the GNU General Public License
    (GPL) Version 2 or the GNU Lesser General Public License.
*/

#define U8_INLINE_IO 1

#include "libu8/libu8.h"

static char versionid[] MAYBE_UNUSED=
  "$Id$";

#include "libu8/u8streamio.h"
#include "libu8/u8ctype.h"
#include "libu8/u8stringfns.h"

#include <stdarg.h>
#include <ctype.h>

/* Core functions */

int _u8_sgetc(u8_byte **s)
{
  return u8_sgetc(s);
}

int _u8_byteoffset(u8_string s,u8_charoff offset,u8_byteoff max)
{
  return u8_byteoffset(s,offset,max);
}

int _u8_charoffset(u8_string s,u8_byteoff i)
{
  return u8_charoffset(s,i);
}

u8_string _u8_strdup(u8_string s)
{
  int len=strlen(s);
  u8_string nstring=u8_malloc(len+1);
  strncpy(nstring,s,len); nstring[len]='\0';
  return nstring;
}
u8_string u8_strndup(u8_string s,int len)
{
  u8_string nstring=u8_malloc(len+1);
  strncpy(nstring,s,len); nstring[len]='\0';
  return nstring;
}

U8_EXPORT
/* u8_strlen_x:
    Arguments: a pointer to a UTF-encoded string and a length
    Returns: an integer indicating the number of unicode characters
in the string it represents
*/
int u8_strlen_x(u8_string str,int slen)
{
  u8_byte *scan=str, *limit=str+slen; int len=0;
  while (scan < limit) {
    len++;
    if (*scan=='\0') {len++; scan++;}
    else u8_sgetc(&scan);}
  return len;
}

U8_EXPORT
/* u8_strlen:
    Arguments: a pointer to a mull-terminated UTF-encoded string
    Returns: an integer indicating the number of unicode characters
in the string it represents
*/
int u8_strlen(u8_string str)
{
  u8_byte *scan=str; int ch=u8_sgetc(&scan), len=0;
  while (ch>=0) {len++; ch=u8_sgetc(&scan);}
  return len;
}

U8_EXPORT
/* u8_substring:
    Arguments: a pointer to a UTF-encoded string and an integer
    Returns: the substring starting at the interger-th character

*/
u8_string u8_substring(u8_string str,int index)
{
  u8_byte *scan=str, *last=scan; int count=index;
  while ((count > 0) && (u8_sgetc(&scan) >= 0)) {
    last=scan; count--;}
  if (count == 0) return last;
  else return NULL;
}

U8_EXPORT
/* u8_slice:
    Arguments: two pointers into a UTF8-encoded string
    Returns: the substring between the pointers
*/
u8_string u8_slice(u8_byte *start,u8_byte *end)
{
  if (end<start) return NULL;
  else if (end-start>65536*8) return NULL;
  else {
    u8_string slice=u8_malloc((end-start)+1);
    strncpy(slice,start,(end-start));
    slice[end-start]='\0';
    return slice;}
}

U8_EXPORT
/* u8_string_ref:
    Arguments: a pointer to a UTF-encoded string
    Returns: returns the first unicode character in the string

*/
int u8_string_ref(u8_byte *str)
{
  int c=u8_sgetc(&str);
  return c;
}

/** UTF-8 lengths and validation **/

static int get_utf8_size(u8_byte s1)
{
  if (s1 < 0x80) return 1;
  else if (s1 < 0xC0) return -1;
  else if (s1 < 0xE0) return 2;
  else if (s1 < 0xF0) return 3;
  else if (s1 < 0xF8) return 4;
  else if (s1 < 0xFC) return 5;
  else if (s1 < 0xFE) return 6;
  else return -1;
}

static int check_utf8_ptr(u8_byte *s,int size)
{
  int i=1;
  if (size == 1) return size;
  /* Now check that the string is valid */
  while (i < size)
    if (s[i] < 0x80) return -i;
    else if (s[i] > 0xc0) return -i;
    else i++;
  return size;
}

static int valid_utf8p(u8_byte *s)
{
  int sz=check_utf8_ptr(s,get_utf8_size(*s));
  while (sz > 0)
    if (*s == '\0') return 1;
    else {
      s=s+sz; sz=check_utf8_ptr(s,get_utf8_size(*s));}
  return 0;
}

U8_EXPORT
/* u8_validptr:
    Arguments: a possible utf8 string
    Returns: 1 if the string is valid, 0 otherwise.
*/
int u8_validptr(u8_byte *s)
{
  int sz=get_utf8_size(*s);
  if (sz>0) return (check_utf8_ptr(s,sz)>0);
  else return 0;
}

U8_EXPORT
/* u8_validp:
    Arguments: a possible utf8 string
    Returns: 1 if the string is valid, 0 otherwise.
*/
int u8_validp(u8_byte *s)
{
  return valid_utf8p(s);
}

U8_EXPORT
/* u8_validate:
    Arguments: a possible utf8 string
    Returns: the number of bytes which are valid
*/
int u8_validate(u8_byte *s,int len)
{
  int sz=get_utf8_size(*s);
  u8_byte *limit=s+len, *start=s;
  while ((s<limit) && (sz=get_utf8_size(*s))) 
    if (s+sz>limit)
      return s-start;
    else s=s+sz;
  return s-start;
}  

U8_EXPORT
/* u8_valid_copy:
     Input: a string which should be UTF-8 encoded
     Output: a utf-8 encoding string
Copies its argument, converting invalid UTF-8 sequences into
sequences of latin-1 characters. This always returns a valid UTF8 
string. */
u8_string u8_valid_copy(u8_byte *s)
{
  U8_OUTPUT out; U8_INIT_OUTPUT(&out,32);
  while (*s)
    if (*s<0x80) u8_putc(&out,*s++);
    else if (check_utf8_ptr(s,get_utf8_size(*s))>0) {
      int c=u8_sgetc(&s); u8_putc(&out,c);}
    else while (*s>=0x80) u8_putc(&out,*s++);
  return out.u8_outbuf;
}

U8_EXPORT
/* u8_valid_copy:
     Input: a string which should be UTF-8 encoded
     Output: a utf-8 encoding string
Copies its argument, converting invalid UTF-8 sequences into
sequences of latin-1 characters. This always returns a valid UTF8 
string. */
u8_string u8_convert_crlfs(u8_byte *s)
{
  U8_OUTPUT out; U8_INIT_OUTPUT(&out,32);
  while (*s)
    if (*s=='\r')
      if (s[1]=='\n') {u8_putc(&out,'\n'); s=s+2;}
      else u8_putc(&out,*s++);
    else if (*s<0x80) u8_putc(&out,*s++);
    else if (check_utf8_ptr(s,get_utf8_size(*s))>0) {
      int c=u8_sgetc(&s); u8_putc(&out,c);}
    else while (*s>=0x80) u8_putc(&out,*s++);
  return out.u8_outbuf;
}

/* Additional functions */

U8_EXPORT
/* u8_downcase:
    Arguments: a null-terminated utf-8 C string
    Returns: a copy of the string in lowercase

 */
u8_string u8_downcase (u8_string string)
{
  u8_byte *scan=string;
  struct U8_OUTPUT ss; int c;
  U8_INIT_OUTPUT(&ss,32);
  while (*scan) {
    if (*scan < 0x80) c=tolower(*scan++);
    else c=u8_tolower(u8_sgetc(&scan));
    u8_putc(&ss,c);}
  return ss.u8_outbuf;
}

U8_EXPORT
/* u8_upcase:
    Arguments: a null-terminated utf-8 C string
    Returns: a copy of the string in uppercase
*/
u8_string u8_upcase (u8_string string)
{
  u8_byte *scan=string;
  struct U8_OUTPUT ss; int c;
  U8_INIT_OUTPUT(&ss,32);
  while (*scan) {
    if (*scan < 0x80) c=toupper(*scan++);
    else c=u8_toupper(u8_sgetc(&scan));
    u8_putc(&ss,c);}
  return ss.u8_outbuf;
}

U8_EXPORT
/* u8_string_append:
    Arguments: a series of strings, terminated by a NULL pointer
    Returns: the concatenation of the strings
*/
u8_string u8_string_append(u8_string first_string,...)
{
  struct U8_OUTPUT out; va_list args; u8_string each;
  U8_INIT_OUTPUT(&out,512);
  va_start(args,first_string);
  u8_puts(&out,first_string);
  while ((each=va_arg(args,u8_string))) u8_puts(&out,each);
  return out.u8_outbuf;
}

U8_EXPORT
/* u8_string_append:
    Arguments: a series of strings, terminated by a NULL pointer
    Returns: the concatenation of the strings
*/
u8_string u8_string_subst(u8_string input,u8_string key,u8_string replace)
{
  u8_byte *scan=input, *next=strstr(scan,key);
  if (next==NULL) return u8_strdup(input);
  else {
    struct U8_OUTPUT out;
    int key_len=u8_bytelen(key), replace_len=u8_bytelen(replace);
    while (next) {
      U8_INIT_OUTPUT(&out,u8_bytelen(input)+(replace_len*4));
      u8_putn(&out,scan,next-scan);
      u8_putn(&out,replace,replace_len);
      scan=scan+key_len;
      next=strstr(scan,key);}
    u8_puts(&out,scan);
    return out.u8_outbuf;}
}


/* The CVS log for this file
   $Log: stringfns.c,v $
   Revision 1.12  2005/12/17 14:19:03  haase
   Handle NULs in utf8 strlen

   Revision 1.11  2005/08/21 19:02:43  haase
   Made functions work with largefiles

   Revision 1.10  2005/05/21 00:26:46  haase
   Fix u8_ispunct to include SYMBOL and MARK

   Revision 1.9  2005/04/02 16:15:20  haase
   Rationalized stringfns/stringio divide and made u8_valid_copy treat invalid utf8 as latin1

   Revision 1.8  2005/03/04 04:02:02  haase
   Various cleanups and minor reorganizations

   Revision 1.7  2005/02/15 02:38:03  haase
   Various fixes discovered during integration into eFramerD.

   Revision 1.6  2005/02/15 00:00:50  haase
   Reorganized libu8 to make xfiles work more cleanly

   Revision 1.5  2005/02/12 03:38:47  haase
   Added copyrights and in-file CVS info


*/
