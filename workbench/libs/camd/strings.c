/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: 
*/

#include <exec/rawfmt.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include "camd_intern.h"

ULONG mystrlen(char *string){
	ULONG ret=0;
	while(string[ret]!=0) ret++;
	return ret;
}

BOOL mystrcmp(char *one,char *two){
  while(*one==*two){
    if(*one==0) return TRUE;
    one++;
    two++;
  }
  return FALSE;
}

char *findonlyfilename(char *pathfile){
  char *temp=pathfile;
  while(*pathfile!=0){
    if(*pathfile=='/') temp=pathfile+1;
    if(*pathfile==':') temp=pathfile+1;
    pathfile++;
  }
  return temp;
}

#ifdef __amigaos4__
ASM void stuffChar( REG(d0, UBYTE in),REG(a3, char **stream)){
	stream[0]++;
	stream[0][-1]=in;
}
#endif


#ifndef __amigaos4__
/*
   Use exec's builtin string formatter instead of a private PutChProc.
   The old stuffChar callback was declared with ASM/REG(d0)/REG(a3)
   annotations, which are empty macros on AROS gcc: RawDoFmt passed the
   character in d0 and the stream pointer in a3 but the compiled callback
   read its arguments from the stack, so nothing was ever written and every
   formatted string (driver paths, cluster names) stayed empty. That made
   InitCamd's DEVS:Midi scan load no drivers and create no clusters.
*/
void mysprintf(struct CamdBase *CamdBase,char *string,char *fmt,...){
	void *start=&fmt+1;

	RawDoFmt(
		 fmt,
		 (RAWARG)start,
		 RAWFMTFUNC_STRING,
		 string
		 );
}
#endif

