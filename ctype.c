/* -*- Mode: C; -*- */

/* Copyright (C) 2004-2011 beingmeta, inc.
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

#include "libu8/u8stringfns.h"
#include "libu8/u8ctype.h"
#include "decomp.h"

#include <ctype.h>

const struct U8_CHARINFO_TABLE *u8_extra_charinfo=NULL;

static unsigned char basic_charinfo[]={
 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, /* 0x0 to 0xf */ 
 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, /* 0x10 to 0x1f */ 
 0xca,0xaa,0xba,0xaa,0xaa,0xab,0xa9,0xaa, /* 0x20 to 0x2f */ 
 0x77,0x77,0x77,0x77,0x77,0xaa,0xbb,0xba, /* 0x30 to 0x3f */ 
 0xa2,0x22,0x22,0x22,0x22,0x22,0x22,0x22, /* 0x40 to 0x4f */ 
 0x22,0x22,0x22,0x22,0x22,0x2a,0xaa,0xb9, /* 0x50 to 0x5f */ 
 0xb1,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x60 to 0x6f */ 
 0x11,0x11,0x11,0x11,0x11,0x1a,0xba,0xbc, /* 0x70 to 0x7f */ 
 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, /* 0x80 to 0x8f */ 
 0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc, /* 0x90 to 0x9f */ 
 0xca,0xbb,0xbb,0xbb,0xbb,0x1a,0xba,0xbb, /* 0xa0 to 0xaf */ 
 0xbb,0x88,0xb1,0xba,0xb8,0x1a,0x88,0x8a, /* 0xb0 to 0xbf */ 
 0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22, /* 0xc0 to 0xcf */ 
 0x22,0x22,0x22,0x2b,0x22,0x22,0x22,0x21, /* 0xd0 to 0xdf */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0xe0 to 0xef */ 
 0x11,0x11,0x11,0x1b,0x11,0x11,0x11,0x11, /* 0xf0 to 0xff */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x100 to 0x10f */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x110 to 0x11f */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x120 to 0x12f */ 
 0x21,0x21,0x21,0x21,0x12,0x12,0x12,0x12, /* 0x130 to 0x13f */ 
 0x12,0x12,0x12,0x12,0x11,0x21,0x21,0x21, /* 0x140 to 0x14f */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x150 to 0x15f */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x160 to 0x16f */ 
 0x21,0x21,0x21,0x21,0x22,0x12,0x12,0x11, /* 0x170 to 0x17f */ 
 0x12,0x21,0x21,0x22,0x12,0x22,0x11,0x22, /* 0x180 to 0x18f */ 
 0x22,0x12,0x21,0x22,0x21,0x11,0x22,0x12, /* 0x190 to 0x19f */ 
 0x21,0x21,0x21,0x22,0x12,0x11,0x21,0x22, /* 0x1a0 to 0x1af */ 
 0x12,0x22,0x12,0x12,0x21,0x15,0x21,0x11, /* 0x1b0 to 0x1bf */ 
 0x55,0x55,0x23,0x12,0x31,0x23,0x12,0x12, /* 0x1c0 to 0x1cf */ 
 0x12,0x12,0x12,0x12,0x12,0x12,0x11,0x21, /* 0x1d0 to 0x1df */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x1e0 to 0x1ef */ 
 0x12,0x31,0x21,0x22,0x21,0x21,0x21,0x21, /* 0x1f0 to 0x1ff */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x200 to 0x20f */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x210 to 0x21f */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x220 to 0x22f */ 
 0x21,0x21,0x11,0x10,0,0,0,0, /* 0x230 to 0x23f */ 
 0,0,0,0,0,0,0,0, /* 0x240 to 0x24f */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x250 to 0x25f */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x260 to 0x26f */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x270 to 0x27f */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x280 to 0x28f */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x290 to 0x29f */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x2a0 to 0x2af */ 
 0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44, /* 0x2b0 to 0x2bf */ 
 0x44,0xbb,0xbb,0x44,0x44,0x44,0x44,0x44, /* 0x2c0 to 0x2cf */ 
 0x44,0xbb,0xbb,0xbb,0xbb,0xbb,0xbb,0xbb, /* 0x2d0 to 0x2df */ 
 0x44,0x44,0x4b,0xbb,0xbb,0xbb,0xbb,0x4b, /* 0x2e0 to 0x2ef */ 
 0xbb,0xbb,0xbb,0xbb,0xbb,0xbb,0xbb,0xbb, /* 0x2f0 to 0x2ff */ 
 0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66, /* 0x300 to 0x30f */ 
 0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66, /* 0x310 to 0x31f */ 
 0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66, /* 0x320 to 0x32f */ 
 0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66, /* 0x330 to 0x33f */ 
 0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66, /* 0x340 to 0x34f */ 
 0x66,0x66,0x66,0x66,0,0,0,0x66, /* 0x350 to 0x35f */ 
 0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66, /* 0x360 to 0x36f */ 
 0,0,0xbb,0,0,0x40,0,0xa0, /* 0x370 to 0x37f */ 
 0,0,0xbb,0x2a,0x22,0x20,0x20,0x22, /* 0x380 to 0x38f */ 
 0x12,0x22,0x22,0x22,0x22,0x22,0x22,0x22, /* 0x390 to 0x39f */ 
 0x22,0,0x22,0x22,0x22,0x22,0x11,0x11, /* 0x3a0 to 0x3af */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11, /* 0x3b0 to 0x3bf */ 
 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x10, /* 0x3c0 to 0x3cf */ 
 0x11,0x22,0x21,0x11,0x21,0x21,0x21,0x21, /* 0x3d0 to 0x3df */ 
 0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21, /* 0x3e0 to 0x3ef */ 
 0x11,0x11,0x21,0xb2,0x12,0x21,0,0};

static short basic_chardata[]={
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x0 to 0xf */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x10 to 0x1f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x20 to 0x2f */ 
 0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0, /* 0x30 to 0x3f */ 
 0,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32, /* 0x40 to 0x4f */ 
 32,32,32,32,32,32,32,32,32,32,32,0,0,0,0,0, /* 0x50 to 0x5f */ 
 0,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32, /* 0x60 to 0x6f */ 
 -32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,0,0,0,0,0, /* 0x70 to 0x7f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x80 to 0x8f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x90 to 0x9f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xa0 to 0xaf */ 
 0,0,0,0,0,743,0,0,0,0,0,0,0,0,0,0, /* 0xb0 to 0xbf */ 
 32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32, /* 0xc0 to 0xcf */ 
 32,32,32,32,32,32,32,0,32,32,32,32,32,32,32,0, /* 0xd0 to 0xdf */ 
 -32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32, /* 0xe0 to 0xef */ 
 -32,-32,-32,-32,-32,-32,-32,0,-32,-32,-32,-32,-32,-32,-32,121, /* 0xf0 to 0xff */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x100 to 0x10f */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x110 to 0x11f */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x120 to 0x12f */ 
 -199,-232,1,-1,1,-1,1,-1,0,1,-1,1,-1,1,-1,1, /* 0x130 to 0x13f */ 
 -1,1,-1,1,-1,1,-1,1,-1,0,1,-1,1,-1,1,-1, /* 0x140 to 0x14f */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x150 to 0x15f */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x160 to 0x16f */ 
 1,-1,1,-1,1,-1,1,-1,-121,1,-1,1,-1,1,-1,-300, /* 0x170 to 0x17f */ 
 0,210,1,-1,1,-1,206,1,-1,205,205,1,-1,0,79,202, /* 0x180 to 0x18f */ 
 203,1,-1,205,207,97,211,209,1,-1,0,0,211,213,130,214, /* 0x190 to 0x19f */ 
 1,-1,1,-1,1,-1,218,1,-1,218,0,0,1,-1,218,1, /* 0x1a0 to 0x1af */ 
 -1,217,217,1,-1,1,-1,219,1,-1,0,0,1,-1,0,56, /* 0x1b0 to 0x1bf */ 
 0,0,0,0,2,1,-2,2,1,-2,2,1,-2,1,-1,1, /* 0x1c0 to 0x1cf */ 
 -1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,-79,1,-1, /* 0x1d0 to 0x1df */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x1e0 to 0x1ef */ 
 0,2,1,-2,1,-1,-97,-56,1,-1,1,-1,1,-1,1,-1, /* 0x1f0 to 0x1ff */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x200 to 0x20f */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x210 to 0x21f */ 
 -130,0,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x220 to 0x22f */ 
 1,-1,1,-1,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x230 to 0x23f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x240 to 0x24f */ 
 0,0,0,-210,-206,0,-205,-205,0,-202,0,-203,0,0,0,0, /* 0x250 to 0x25f */ 
 -205,0,0,-207,0,0,0,0,-209,-211,0,0,0,0,0,-211, /* 0x260 to 0x26f */ 
 0,0,-213,0,0,-214,0,0,0,0,0,0,0,0,0,0, /* 0x270 to 0x27f */ 
 -218,0,0,-218,0,0,0,0,-218,0,-217,-217,0,0,0,0, /* 0x280 to 0x28f */ 
 0,0,-219,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x290 to 0x29f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2a0 to 0x2af */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2b0 to 0x2bf */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2c0 to 0x2cf */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2d0 to 0x2df */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2e0 to 0x2ef */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2f0 to 0x2ff */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x300 to 0x30f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x310 to 0x31f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x320 to 0x32f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x330 to 0x33f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x340 to 0x34f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x350 to 0x35f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x360 to 0x36f */ 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x370 to 0x37f */ 
 0,0,0,0,0,0,38,0,37,37,37,0,64,0,63,63, /* 0x380 to 0x38f */ 
 0,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32, /* 0x390 to 0x39f */ 
 32,32,0,32,32,32,32,32,32,32,32,32,-38,-37,-37,-37, /* 0x3a0 to 0x3af */ 
 0,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32,-32, /* 0x3b0 to 0x3bf */ 
 -32,-32,-31,-32,-32,-32,-32,-32,-32,-32,-32,-32,-64,-63,-63,0, /* 0x3c0 to 0x3cf */ 
 -62,-57,0,0,0,-47,-54,0,1,-1,1,-1,1,-1,1,-1, /* 0x3d0 to 0x3df */ 
 1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1, /* 0x3e0 to 0x3ef */ 
 -86,-80,7,0,-60,-96,0,1,-1,-7,1,-1,0,0,0,0};

int u8_charinfo_size=1024;
const unsigned char *u8_charinfo=basic_charinfo;
const short *u8_chardata=basic_chardata;

U8_EXPORT int u8_lookup_charinfo(int c)
{
  return U8_SYMBOL;
}
U8_EXPORT int u8_lookup_chardata(int c)
{
  return 0;
}

U8_EXPORT void u8_set_charinfo(int n,unsigned char *info,short *data)
{
  u8_charinfo=info;
  u8_chardata=data;
  u8_charinfo_size=n;
}

/** Canonical Decomposition functions **/

static struct U8_DECOMPOSITION *lookup_char(unsigned int ch)
{
  int bot=0, top=n_decompositions-1;
  while (top >= bot) {
    int mid=bot+(top-bot)/2;
    if (decompositions[mid].code == ch)
      return &(decompositions[mid]);
    else if (ch < decompositions[mid].code) top=mid-1;
    else bot=mid+1;}
  return NULL;
}

#if 0
static int compare_decomps(const void *vx,const void *vy)
{
  struct U8_DECOMPOSITION **x=(struct U8_DECOMPOSITION **)vx;
  struct U8_DECOMPOSITION **y=(struct U8_DECOMPOSITION **)vy;
  return strcmp((*x)->decomp,(*y)->decomp);
}

static void setup_decomposition_tables()
{
  int i=0;
  while (decompositions[n_decompositions].code)
    n_decompositions++;
  decomp_table=
    fd_malloc(sizeof(struct U8_DECOMPOSITION *)*
	      n_decompositions);
  while (i < n_decompositions) {
    decomp_table[i]=&(decompositions[i]); i++;}
  qsort(decomp_table,n_decompositions,
	sizeof(struct U8_UNICODE_DECOMPOSITION *),
	compare_decomps);
}

static struct U8_DECOMPOSITION *lookup_decomp(char *ch)
{
  int bot=0, top=n_decompositions-1;
  while (top >= bot) {
    int mid=bot+(top-bot)/2;
    int cmp=strcmp(decompositions[mid].decomp,ch);
    if (cmp == 0)
      return &(decompositions[mid]);
    else if (cmp < 0) top=mid-1;
    else bot=mid+1;}
  return NULL;
}

U8_EXPORT int u8_recompose_char(fd_u8char *s)
{
  struct U8_DECOMPOSITION *ud=lookup_decomp(s);
  if (ud) return ud->code;
  else return -1;
}
#endif

U8_EXPORT u8_string u8_decompose_char(unsigned int ch)
{
  struct U8_DECOMPOSITION *ud=lookup_char(ch);
  if (ud) return ud->decomp;
  else return NULL;
}

U8_EXPORT int u8_base_char(unsigned int ch)
{
  struct U8_DECOMPOSITION *ud=lookup_char(ch);
  if (ud) {
    u8_string decomp=ud->decomp; int c=u8_sgetc(&decomp);
    return c;}
  else return ch;
}

/* Entity interpretation */

/* This converts XHTML 4.01 entities into their Unicode codepoint. */

struct ENTITY_TABLE {char *name; int code;};

#include "entities.h"

U8_EXPORT int u8_entity2code(u8_string entity)
{
  int top=N_ENTITIES, bot=0, middle=N_ENTITIES/2;
  int cmp=1;
  while ((top>=bot) && (middle<N_ENTITIES) && (middle>=0) && (cmp!=0)) {
    cmp=strcmp(entity,entities[middle].name);
    if (cmp<0) {
      top=middle-1; middle=(bot+top)/2;}
    else if (cmp>0) {
      bot=middle+1; middle=(bot+top)/2;}
    else if (cmp==0) break;}
  if (cmp==0) return entities[middle].code;
  else return -1;
}

U8_EXPORT u8_string u8_code2entity(int code)
{
  int i=0; while (i<N_ENTITIES)
    if (entities[i].code==code)
      return entities[i].name;
    else i++;
  return NULL;
}

static u8_string entity_end(u8_string entity)
{
  u8_string scan=entity; int base=0;
  if ((*scan=='#') && (scan[1]=='x')) {
    scan=scan+2; base=16;}
  else if (*scan=='#') {
    scan=scan+1; base=10;}
  while ((*scan) && (*scan!=';'))
    if (base)
      if ((base==10) && (isdigit(*scan))) scan++;
      else if ((base==16) && (isxdigit(*scan))) scan++;
      else return NULL;
    else scan++;
  return scan;
}

U8_EXPORT int u8_parse_entity(u8_byte *entity,u8_byte **endp)
{
  int codepoint;
  u8_byte buf[16];
  u8_byte *end=*endp, *semi=entity_end(entity);
  if (semi==NULL) return -1;
  else if (*semi=='\0') {
    *endp=semi; return -1;}
  else if ((end) && (semi>end)) {
    *endp=NULL; return -1;}
  else if ((semi-entity)>15) {
    *endp=NULL; return -1;}
  if (*entity=='#') {
    u8_byte *start; int base;
    if (entity[1]=='x') {
      start=entity+2; base=16;}
    else {start=entity+1; base=10;}
    strncpy(buf,start,semi-start); buf[semi-start]='\0';
    codepoint=strtol(buf,NULL,base);}
  else {
    strncpy(buf,entity,semi-entity); buf[semi-entity]='\0';
    codepoint=u8_entity2code(buf);}
  if (codepoint>0) {
    *endp=semi+1; return codepoint;}
  else {*endp=NULL; return -1;}
}

