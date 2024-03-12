#include <stdarg.h>
#include <stdio.h>
#include <nds.h>

#include "nds_debug.h"

int debug_mode = 0;

void nds_debug_print(char *file, int line, char *fmt, ...)
{
  char buffer[1024];
  va_list ap;

  va_start(ap, fmt);

  if (debug_mode) {
    sprintf(buffer, "%s:%d - %s", file, line, fmt);
  } else {
    sprintf(buffer, "%s", fmt);
  }

  viprintf(buffer, ap);

  va_end(ap);
}
