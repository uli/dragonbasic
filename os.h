#ifndef _OS_H
#define _OS_H

#ifdef __WIN32__
#include <libgen.h>
#include <windows.h>
#define PATHSEP "\\"
#define EXE ".exe"
#define OS_getAppDir(buf) do { GetModuleFileNameA(0, (buf), 0x100); dirname(buf); } while(0)
#else
#define OS_getAppDir(buf) do { strcpy(buf, APPDIR); } while(0)
#define PATHSEP "/"
#define EXE ""
#endif

static const char* stolower(const char *str)
{
  static char buf[256];
  char *b = buf;
  while (*str)
    *b++ = tolower(*str++);
  *b = 0;
  return buf;
}

#endif
