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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "getfilecontents.h"
#include "helpers.h"
#include "libeconf.h"

static econf_err
store (econf_file *ef, const char *group, const char *key,
       const char *value, uint64_t line_number)
{
  if (ef->alloc_length == ef->length) {
    struct file_entry *tmp;

    tmp = realloc(ef->file_entry, (ef->length + 1) * sizeof(struct file_entry));
    if (!tmp)
      return ECONF_NOMEM;
    ef->file_entry = tmp;
    ef->length++;
    ef->alloc_length = ef->length;
  }

  ef->file_entry[ef->length-1].line_number = line_number;

  if (group)
    ef->file_entry[ef->length-1].group = strdup(group);
  else
    ef->file_entry[ef->length-1].group = strdup(KEY_FILE_NULL_VALUE);

  if (key)
    ef->file_entry[ef->length-1].key = strdup(key);
  else
    ef->file_entry[ef->length-1].key = strdup(KEY_FILE_NULL_VALUE);

  if (value)
    ef->file_entry[ef->length-1].value = strdup(value);
  else
    ef->file_entry[ef->length-1].value = NULL;

  return ECONF_SUCCESS;
}

/* Read the file line by line and parse for comments, keys and values */
econf_err
read_file(econf_file *ef, const char *file,
	  const char *delim, const char *comment)
{
  char buf[BUFSIZ];
  char *current_group = NULL;
  econf_err retval = ECONF_SUCCESS;
  uint64_t line = 0;

  FILE *kf = fopen(file, "rbe");

  if (kf == NULL)
    return ECONF_NOFILE;

  ef->path = strdup (file);
  if (ef->path == NULL) {
    fclose (kf);
    return ECONF_NOMEM;
  }

  while (fgets(buf, sizeof(buf), kf)) {
    char *p, *name, *data = NULL;

    line++;

    if (*buf == '\n')
      continue; /* ignore empty lines */

    /* go throug all comment characters and check, if one of could be found */
    for (size_t i = 0; i < strlen(comment); i++) {
      p = strchr(buf, comment[i]);
      if (p)
	*p = '\0';
    }

    /* Remove trailing newline character */
    size_t n = strlen(buf);
    if (n && *(buf + n - 1) == '\n')
      *(buf + n - 1) = '\0';

    if (!*buf)
      continue;       /* empty line */

    /* ignore space at begin of the line */
    name = buf;
    while (*name && isspace((unsigned)*name))
      name++;

    /* check for groups */
    if (name[0] == '[') {
      p = name + strlen(name) - 1;
      /* XXX Remove [] around group name */
      while (isspace (*p)) p--;
      if (*p != ']') {
	retval = ECONF_PARSE_ERROR;
	goto out;
      }
      p++;
      *p = '\0';
      if (current_group)
	free (current_group);
      current_group = strdup (name);
      continue;
    }

    /* go to the end of the name */
    data = name;
    while (*data && !(isspace((unsigned)*data) ||
		      strchr(delim, *data) != NULL))
      data++;
    if (data > name && *data)
      *data++ = '\0';

    if (!*name || data == name)
      continue;

    if (*data == '\0')
      /* No seperator -> return NULL pointer, there is no value,
	 not even an empty key */
      data = NULL;
    else {
      /* go to the begin of the value */
      while (*data
	     && (isspace((unsigned)*data) || strchr(delim, *data) != NULL
		 || *data == '"'))
	data++;

      /* remove space at the end of the value */
      p = data + strlen(data);
      if (p > data)
	p--;
      while (p > data && (isspace((unsigned)*p) || *p == '"'))
	*p-- = '\0';
    }

    retval = store(ef, current_group, name, data, line);
    if (retval)
      goto out;
  }

 out:
  fclose (kf);
  if (current_group)
    free (current_group);

  return retval;
}
