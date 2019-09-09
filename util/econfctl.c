/*
  Copyright (C) 2019 SUSE LLC
  Author: Dominik Gedon <dgedon@suse.com>

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/libeconf.h"


#define EDITOR getenv("EDITOR")


static void usage();




int main (int argc, char *argv[]) {
	econf_file *key_file = NULL;
	econf_err error;
	
	if (argc < 2) {
                usage("Missing option!\n");

	// econfctl show 	
	} else if (strcmp(argv[1], "show") == 0) {
		if (argc < 3) {
			usage("Missing filename!\n");
		}
		if (argc >= 4) {
			usage("Too many arguments!\n");
		}	

		// suffix for econf_ReadDirs()
		char *suffix = strtok(argv[2], ".");
		int position = 0;
		char *suffix_array[2];
        	while (suffix != NULL) {
			suffix_array[position++] = suffix;
                	suffix = strtok(NULL, ".");
        	}
		
		// process given configuration file directories
		if ((error = econf_readDirs(&key_file, "/usr/etc", "/etc", argv[2], suffix_array[1], "=", "#"))) {
			fprintf(stderr, "%s\n", econf_errString(error));
		}

		// show groups, keys and their value
		char **groups = NULL;
		size_t group_count = 0;
		size_t key_count = 0;
                char **keys = NULL;
		char *value = NULL;

		if ((error = econf_getGroups(key_file, &group_count, &groups))) {
			fprintf(stderr, "%s\n", econf_errString(error));
		}
		
		for (size_t g = 0; g < group_count; g++) {
			if ((error = econf_getKeys(key_file, groups[g], &key_count, &keys))) {
				fprintf(stderr, "%s\n", econf_errString(error));
			}
			printf("%s\n", groups[g]);
			for (size_t k = 0; k < key_count; k++) {                		
				if ((error = econf_getStringValue(key_file, groups[g], keys[k], &value)) || value == NULL || strlen(value) == 0) {
					fprintf(stderr, "%s\n", econf_errString(error));
				}
				printf("%s = %s\n", keys[k], value);
			}
			printf("\n");
		}
		// free created pointers
		econf_free(groups);
		econf_free(keys);
		free(value);
		
		
		free(suffix);

	// econfctl cat
	} else if (strcmp(argv[1], "cat") == 0) {
		if (argc < 3) {
                        usage("Missing filename!\n");
		}
                if (argc >= 4) {
                        usage("Too many arguments!\n");
                }

	// econfctl edit
	} else if (strcmp(argv[1], "edit") == 0) {
		if (argc < 3) {
                        usage("Missing filename!\n");
                }
		if (argc >= 4) {
                        usage("Too many arguments!\n");
                }


	// econfctl revert
	} else if (strcmp(argv[1], "revert") == 0) {
		if (argc < 3) {
                        usage("Missing filename!\n");
                }
		if (argc >= 4) {
                        usage("Too many arguments!\n");
                }


	
	} else {
		usage("Unknown option!\n");
	}
	
	econf_free(key_file);
	return EXIT_SUCCESS;
}



static void usage(char *message) {
	fprintf(stderr,"%s\n", message);
	fprintf(stderr, "Usage: econfctl [ OPTIONS ] filename.conf\n\n"
		"OPTIONS:\n"
		"show     reads all snippets for filename.conf and prints all groups, keys and their values\n"
		"cat      prints the content and the name of the file in the order as read by econf_readDirs\n"
		"edit     starts the editor EDITOR (environment variable) where the groups, keys and values can be modified and saved afterwards\n"
		"            --full:   copy the original configuration file to /etc instead of creating drop-in files\n"
		"            --force:  if the configuration file does not exist, create a new one\n"
		"revert   reverts all changes to the vendor versions. Basically deletes the config files in /etc\n\n");
	exit(EXIT_SUCCESS);
}








