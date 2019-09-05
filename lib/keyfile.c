/*
  Copyright (C) 2019 SUSE LLC
  Author: Pascal Arlt <parlt@suse.com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "libeconf.h"
#include "../include/defines.h"
#include "../include/helpers.h"
#include "../include/keyfile.h"

#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

econf_err key_file_append(econf_file *kf) {
  /* XXX check return values and for NULL pointers */
  if(kf->length++ >= kf->alloc_length) {
    kf->alloc_length++;
    kf->file_entry =
      realloc(kf->file_entry, (kf->alloc_length) * sizeof(struct file_entry));
    initialize(kf, kf->alloc_length - 1);
  }
  return ECONF_SUCCESS;
}

/* --- GETTERS --- */

/* XXX all get*ValueNum functions are missing error handling */
econf_err getIntValueNum(econf_file key_file, size_t num, int32_t *result) {
  *result = strtol(key_file.file_entry[num].value, NULL, 10);
  return ECONF_SUCCESS;
}

econf_err getInt64ValueNum(econf_file key_file, size_t num, int64_t *result) {
  *result = strtoll(key_file.file_entry[num].value, NULL, 10);
  return ECONF_SUCCESS;
}

econf_err getUIntValueNum(econf_file key_file, size_t num, uint32_t *result) {
  *result = strtoul(key_file.file_entry[num].value, NULL, 10);
  return ECONF_SUCCESS;
}

econf_err getUInt64ValueNum(econf_file key_file, size_t num, uint64_t *result) {
  *result = strtoull(key_file.file_entry[num].value, NULL, 10);
  return ECONF_SUCCESS;
}

econf_err getFloatValueNum(econf_file key_file, size_t num, float *result) {
  *result = strtof(key_file.file_entry[num].value, NULL);
  return ECONF_SUCCESS;
}

econf_err getDoubleValueNum(econf_file key_file, size_t num, double *result) {
  *result = strtod(key_file.file_entry[num].value, NULL);
  return ECONF_SUCCESS;
}

econf_err getStringValueNum(econf_file key_file, size_t num, char **result) {
  if (key_file.file_entry[num].value)
    *result = strdup(key_file.file_entry[num].value);
  else
    *result = NULL;

  return ECONF_SUCCESS;
}

econf_err getBoolValueNum(econf_file key_file, size_t num, bool *result) {
  char *value, *tmp;
  tmp = strdupa(key_file.file_entry[num].value);
  value = toLowerCase(tmp);
  size_t hash = hashstring(toLowerCase(key_file.file_entry[num].value));

  if ((*value == '1' && strlen(tmp) == 1) || hash == YES || hash == TRUE)
    *result = true;
  else if ((*value == '0' && strlen(tmp) == 1) || !*value ||
	   hash == NO || hash == FALSE)
    *result = false;
  else
    return ECONF_PARSE_ERROR;

  return ECONF_SUCCESS;
}

/* --- SETTERS --- */

econf_err setGroup(econf_file *key_file, size_t num, const char *value) {
  if (key_file == NULL || value == NULL)
    return ECONF_ERROR;
  if (key_file->file_entry[num].group)
    free(key_file->file_entry[num].group);
  key_file->file_entry[num].group = strdup(value);
  if (key_file->file_entry[num].group == NULL)
    return ECONF_NOMEM;

  return ECONF_SUCCESS;
}

econf_err setKey(econf_file *key_file, size_t num, const char *value) {
  if (key_file == NULL || value == NULL)
    return ECONF_ERROR;
  if (key_file->file_entry[num].key)
    free(key_file->file_entry[num].key);
  key_file->file_entry[num].key = strdup(value);
  if (key_file->file_entry[num].key == NULL)
    return ECONF_NOMEM;

  return ECONF_SUCCESS;
}


#define econf_setValueNum(FCT_TYPE, TYPE, FMT, PR)			\
econf_err set ## FCT_TYPE ## ValueNum(econf_file *ef, size_t num, const void *v) { \
  const TYPE *value = (const TYPE*) v; \
  char *ptr; \
\
  if (asprintf (&ptr, FMT PR, *value) == -1) \
    return ECONF_NOMEM; \
\
  if (ef->file_entry[num].value) \
    free(ef->file_entry[num].value); \
\
  ef->file_entry[num].value = ptr; \
\
  return ECONF_SUCCESS; \
}

econf_setValueNum(Int, int32_t, "%", PRId32)
econf_setValueNum(Int64, int64_t, "%",  PRId64)
econf_setValueNum(UInt, uint32_t, "%", PRIu32)
econf_setValueNum(UInt64, uint64_t, "%", PRIu64)
#define PRFLOAT ,FLT_DECIMAL_DIG
econf_setValueNum(Float, float, "%.*g", PRFLOAT)
#define PRDOUBLE ,DBL_DECIMAL_DIG
econf_setValueNum(Double, double, "%.*g", PRDOUBLE)

econf_err setStringValueNum(econf_file *ef, size_t num, const void *v) {
  const char *value = (const char*) (v ? v : "");
  char *ptr;

  if ((ptr = strdup (value)) == NULL)
    return ECONF_NOMEM;

  if (ef->file_entry[num].value)
    free(ef->file_entry[num].value);

  ef->file_entry[num].value = ptr;

  return ECONF_SUCCESS;
}

/* XXX This needs to be optimised and error checking added */
econf_err setBoolValueNum(econf_file *kf, size_t num, const void *v) {
  const char *value = (const char*) (v ? v : "");
  econf_err error = ECONF_SUCCESS;
  char *tmp = strdup(value);
  size_t hash = hashstring(toLowerCase(tmp));

  if ((*value == '1' && strlen(tmp) == 1) || hash == YES || hash == TRUE) {
    free(kf->file_entry[num].value);
    kf->file_entry[num].value = strdup("true");
  } else if ((*value == '0' && strlen(tmp) == 1) || !*value ||
             hash == NO || hash == FALSE) {
    free(kf->file_entry[num].value);
    kf->file_entry[num].value = strdup("false");
  } else if (hash == KEY_FILE_NULL_VALUE_HASH) {
    free(kf->file_entry[num].value);
    kf->file_entry[num].value = strdup(KEY_FILE_NULL_VALUE);
  } else { error = ECONF_ERROR; }

  free(tmp);
  return error;
}
