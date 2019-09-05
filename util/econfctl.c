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





int main (int argc, char* argv[]) {
        if (argc < 2) {
                usage("Missing option!\n");
	
	} else if (strcmp(argv[1], "show") == 0) {
		if (argc < 3) {
			usage("Missing filename!\n");
		}
			
	} else if (strcmp(argv[1], "cat") == 0) {
		if (argc < 3) {
                        usage("Missing filename!\n");
                }

	} else if (strcmp(argv[1], "edit") == 0) {
		if (argc < 3) {
                        usage("Missing filename!\n");
                }


	} else if (strcmp(argv[1], "revert") == 0) {
		if (argc < 3) {
                        usage("Missing filename!\n");
                }

	
	} else {
		usage("Unknown option!\n");
	}
}



static void usage(char* message) {
	fprintf(stderr,"%s", message);
	fprintf(stderr, "Usage: econfctl [ OPTIONS ] filename.conf\n\n"
		"OPTIONS:\n"
		"show     reads all snippets for filename.conf and prints all groups, keys and their values\n"
		"cat      prints the content and the name of the file in the order as read by econf_readDirs\n"
		"edit     starts the editor EDITOR (environment variable) where the groups, keys and values can be modified and saved afterwards\n"
		"            --full:   copy the original configuration file to /etc instead of creating drop-in files\n"
		"            --force:  if the configuration file does not exist, create a new one\n"
		"revert   reverts all changes to the vendor versions\n");
	exit(1);
}








