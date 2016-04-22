#include "util.h"

#include <ctype.h>
#include <stddef.h>

char *util::strstrip_all(const char *in_str, char *out_str, int len)
{
  if (in_str == NULL || out_str == NULL) 
    return NULL;

  char *s = out_str;
  for (int i = 0; *in_str && i < len; ++in_str) {
    if (::isspace((int)*in_str) == 0)
      *s++ = *in_str;
  }

  return s;
}
