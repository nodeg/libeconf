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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "libeconf.h"

static void usage(const char *);
static void initPath(void);
static void checkPath(const char *);
static void setPath(const char *, bool);
static bool fileExist(const char *);

static char *path = NULL;
static bool path_initialized = false;
static size_t path_length = 0;

int main (int argc, char *argv[]) {
    fprintf(stdout, "|DEBUG Messages ------------------------------------| \n"); /* debug */
    initPath();
    // fprintf(stdout, "|--Sizeof(path): %ld\n", sizeof(path)); /* debug */
    // fprintf(stdout, "|--Strlen(path): %ld\n", strlen(path)); /* debug */

    econf_file *key_file = NULL;
    econf_err error;

    if (argc < 2) {
        usage("Missing command!\n");

    /**
     * @brief This command will read all snippets for example.conf
     *        (econf_readDirs) and print all groups, keys and their
     *        values as an application would see them.
     */
    } else if (strcmp(argv[1], "show") == 0) {
        if (argc < 3) {
            usage("Missing filename!\n");
        }
        if (argc >= 4) {
            usage("Too many arguments!\n");
        }

        /* suffix for econf_ReadDirs() */
        char *suffix = strtok(argv[2], ".");
        int position = 0;
        char *suffix_array[2];

        while (suffix != NULL) {
            suffix_array[position++] = suffix;
            suffix = strtok(NULL, ".");
        }
        /*
         * process given configuration file directories. A wrapper has
         * to be written in order to parse more directories. At this
         * moment only /etc and /usr/etc is handled.
         */
        if ((error = econf_readDirs(&key_file, "/usr/etc", "/etc",
                suffix_array[0], suffix_array[1],"=", "#"))) {
            fprintf(stderr, "%s\n", econf_errString(error));
            econf_free(key_file);
            return EXIT_FAILURE;
        }

        /* TODO: include econf_mergeFiles() */

        char **groups = NULL;
        char *value = NULL;
        size_t group_count = 0;

        /* show groups, keys and their value */
        if ((error = econf_getGroups(key_file, &group_count, &groups))) {
            fprintf(stderr, "%s\n", econf_errString(error));
            econf_free(groups);
            econf_free(key_file);
            return EXIT_FAILURE;
        }
        char **keys = NULL;
        size_t key_count = 0;

        for (size_t g = 0; g < group_count; g++) {
            if ((error = econf_getKeys(key_file, groups[g], &key_count,
                            &keys))) {
                fprintf(stderr, "%s\n", econf_errString(error));
                econf_free(groups);
                econf_free(keys);
                econf_free(key_file);
                return EXIT_FAILURE;
            }
            printf("%s\n", groups[g]);
            for (size_t k = 0; k < key_count; k++) {
                if ((error = econf_getStringValue(key_file, groups[g], keys[k],
                             &value)) || value == NULL || strlen(value) == 0) {
                    fprintf(stderr, "%s\n", econf_errString(error));
                    econf_free(groups);
                    econf_free(keys);
                    econf_free(key_file);
                    free(value);
                    return EXIT_FAILURE;
                }
                printf("%s = %s\n", keys[k], value);
                free(value);
            }
            printf("\n");
            econf_free(keys);
        }
        econf_free(groups);
        //free(value);


    /**
     * @brief This command will print the content of the files and the name of the
     *        file in the order as read by econf_readDirs.
     */
    } else if (strcmp(argv[1], "cat") == 0) {
        if (argc < 3) {
            usage("Missing filename!\n");
        }
        if (argc >= 4) {
             usage("Too many arguments!\n");
        }

    /**
     * @brief This command will start an editor (EDITOR environment variable),
     *        which shows all groups, keys and their values (like econfctl
     *        show output), allows the admin to modify them, and stores the
     *        changes afterwards.
     * --full: copy the original config file to /etc instead of creating drop-in
     *         files.
     * --force: if the config does not exist, create a new one.
     *
     *  TODO:
     *      - Replace static values (path, dir)
     *      - Use fork() to be able to use exec for mkdir and open EDITOR
     *      - Check where the mkdir binary resides (/usr/bin/)
     *      - Delete parent directory when using edit --force as root when the
     *        file was not saved and the directory was created before.
     *      - Work with the information provided by waitpid(2)
     */
    } else if (strcmp(argv[1], "edit") == 0) {
        if (argc < 3) {
            usage("Missing filename!\n");
        } else if (argc > 4) {
            usage("Too many arguments!\n");
        } else {
            /* At the moment with static directory. */
            const char *dir = "/etc/"; /* TODO */
            char *argv2 = argv[2];
            char *home = getenv("HOME");
            bool fileExists = false;
            bool isRoot = false;
            uid_t uid = getuid();
            uid_t euid = geteuid();

            setPath(dir, false);

            fprintf(stdout, "|Applying dir to path\n"); /* debug */
            fprintf(stdout, "|Path content: %s\n", path); /* debug */
            fprintf(stdout, "|Strlen(path): %ld\n", strlen(path)); /* debug */

            setPath(argv2, true);

            const char *editor = getenv("EDITOR");
            //fprintf(stdout, "|--Editor: %s\n", editor); /* debug */
            if(editor == NULL) {
                /* if no editor is specified take vim as default */
                editor = "/usr/bin/vim";
            }

            char *xdgConfigDir = getenv("XDG_CONFIG_HOME");
            if (xdgConfigDir == NULL) {
                /* if no XDG_CONFIG_HOME ist specified take ~/.config as
                 * default
                 */
                xdgConfigDir = strncat(home, "/.config/", sizeof(home) - strlen(home) - 1);
                //fprintf(stdout, "XDG conf dir: %s\n", xdgConfigDir); /* debug */
            } else {
                /* XDG_CONFIG_HOME does not end with an '/', we have to add one manually */
                strncat(path, "/", sizeof(path) - strlen(path) - 1);
            }

            /* check if file already exists */
            fileExists = fileExist(path);
            /* basic write permission checking by using uid/euid */
            if (uid == 0 && uid == euid) {
                isRoot = true;
            } else {
                isRoot = false;
            }

            /* copy the original config file to /etc instead of creating drop-in
             * files */
            if (argc == 4 && strcmp(argv[3], "--full") == 0) {
                fprintf(stdout, "|command: --edit %s --full --> stop\n", argv2); /* debug */
                /* TODO */

            /* if the config file does -not- exist, create it */
            } else if (argc == 4 && strcmp(argv[3], "--force") == 0) {
                fprintf(stdout, "|command: --edit %s --force\n", argv2); /* debug */
                if (!fileExists) {
                    fprintf(stdout, "|-File does not exist\n"); /* debug */
                    if (isRoot) {
                        fprintf(stdout, "|-Root path\n"); /* debug */

                        /* check if file.d directory already exists
                         * and create new one if it does not.
                         */
                        setPath(".d/", true);

                        if (access(path, F_OK) != 0) {
                            if (errno == ENOENT) {
                                /* directory does not exist */
                                fprintf(stdout, "|--Directory does not exist\n"); /* debug */

                                /* create parent directory (filename.d) */
                                pid_t pid = fork();
                                if (pid  == -1) {
                                    fprintf(stderr, "Error during fork().\n");
                                    exit(EXIT_FAILURE);
                                } else if (pid == 0) {
                                    /* child */
                                    fprintf(stdout, "|--Child: create directory using mkdir\n"); /* debug */
                                    execlp("/usr/bin/mkdir", "/usr/bin/mkdir", path, NULL);
                                } else {
                                    /* parent - TODO */
                                    int wstatus = 0;
                                    if(waitpid(pid, &wstatus, 0) == -1) {
                                        fprintf(stderr, "Error using waitpid().\n");
                                        exit(EXIT_FAILURE);
                                    }
                                    if (WIFEXITED(wstatus)) {
                                        WEXITSTATUS(wstatus);
                                    }
                                }
                                /* apply filename to path */
                                setPath(argv2, true);

                                /* TODO: fork() and status check */
                                return execlp(editor, editor, path, NULL);
                            }
                        } else {
                            fprintf(stdout, "|-Directory already exists\n"); /* debug */
                            /* apply filename to path */
                            setPath(argv2, true);
                        }
                        /* TODO: Remove created parent directory if the file was
                         *       not saved, but only if directory is empty
                         */

                    } else { // not root
                        fprintf(stdout, "|-Not root path\n"); /* debug */
                        /* the user is not root and therefore the path has to
                         * be adjusted, since then the file is saved in the HOME
                         * directory of the user.
                         */
                        fprintf(stdout, "|-Overwriting path with XDG_CONF_DIR\n"); /* debug */
                        setPath(xdgConfigDir, false);
                        /* concatenate with argv[2] argument */
                        setPath(argv2, true);

                        fprintf(stdout, "|-Path: %s\n", path); /* debug */
                        fprintf(stdout, "|-Sizeof(path): %ld\n", sizeof(path)); /* debug */
                        fprintf(stdout, "|-Strlen(path): %ld\n", strlen(path)); /* debug */

                        /* TODO: fork() and status check */
                        return execlp(editor, editor, path, NULL);
                    }
                }
                /* the file does already exist. Just open it. */
                fprintf(stdout, "|-File already exists\n"); /* debug */
                fprintf(stdout, "|-Path: %s\n", path); /* debug */

                /* TODO: fork() and status check */
                return execlp(editor, editor, path, NULL);

            } else if (argc == 4 && ((strcmp(argv[3], "--force") != 0)
                                  || (strcmp(argv[3], "--full") != 0))) {
                usage("Unknown command!\n");

            } else if (argc == 3) {
                /* just open vim and let it handle the file */
                fprintf(stdout, "-Normal path\n"); /* debug */
                fprintf(stdout, "-Path: %s\n", path); /* debug */

                /* TODO: fork() and status check */
                return execlp(editor, editor, path, NULL);

            } else {
                usage("Unknown command!\n");
           }
        }


    /**
     * @biref Revert all changes to the vendor versions. In the end this means
     *        most likely to delete all files in /etc for this.
     */
    } else if (strcmp(argv[1], "revert") == 0) {
        if (argc < 3) {
            usage("Missing filename!\n");
        }
        if (argc >= 4) {
            usage("Too many arguments!\n");
        }
    } else {
        usage("Unknown command!\n");
    }
    econf_free(key_file);
    return EXIT_SUCCESS;
}

/**
 * @brief Print error messages and show the usage.
 */
static void usage(const char *message) {
    fprintf(stderr,"%s\n", message);
    fprintf(stderr, "Usage: econfctl [ COMMANDS ] filename.conf\n\n"
        "COMMANDS:\n"
        "show     reads all snippets for filename.conf and prints all groups,"
                  " keys and their values\n"
        "cat      prints the content and the name of the file in the order as"
                  " read by econf_readDirs\n"
        "edit     starts the editor EDITOR (environment variable) where the"
                  " groups, keys and values can be modified and saved afterwards\n"
        "            --full:   copy the original configuration file to /etc"
                               " instead of creating drop-in files\n"
        "            --force:  if the configuration file does not exist, create"
                               " a new one\n"
        "revert   reverts all changes to the vendor versions. Basically deletes"
                  " the config files in /etc\n\n");
    exit(EXIT_FAILURE);
}

/**
 * @brief Checks if a file already exists
 * @param file The file to check
 * @return true if the file already exists
 *         false if the file does not exist
 */
static bool fileExist(const char *file) {
    if (access(file, F_OK) != -1) {
        return true;
    } else {
        return false;
    }
}
/**
 * @brief Initializes the path array
 * Initializes the path array with a default length.
 */
static void initPath() {
    fprintf(stdout, "|initPath\n"); /* debug */
     path = (char *) calloc(40, sizeof(char));
     if (path != NULL) {
         path_length = 40;
         path_initialized = true;
     } else {
         fprintf(stderr, "Error using calloc!");
         exit(EXIT_FAILURE);
     }
}

/**
  * @brief Check the path length and increase it if necessary.
  * Checks if path is big enough to fit the argument. If not,
  * path will be increased to the length of arg + 1 and the
  * path_length variable will be adjusted accordingly.
  *
  * @param arg The string whose length will be checked.
  *
  */
static void checkPath(const char *value) {
    if (path_length <= strlen(value)) {
        /* path is to small and has to be increased */
        int *error = realloc(path, (strlen(value) + 1) * sizeof(char));
        if (error == NULL) {
            fprintf(stderr, "Error using realloc!");
            exit(EXIT_FAILURE);
        }
        path_length = strlen(value) + 1;
    }
}

/**
  * @brief Sets the path according to the input
  * First of all the function checkPath() is called to verify the
  * length of path is sufficient. Then either the given string arg
  * is concatenated with the existing path, or path is overwritten
  * with arg.
  *
  * @param value The string who is concatenated with the path or who
  *              overwrites the path.
  * @param concatenate Bool value to decice if arg is concatenated
  *                    with path or overwrites path.
  *                    true: arg is concatenated with existing path
  *                    false: arg overwrites the existing path
  */
static void setPath(const char *value, bool concatenate) {
    /* checks if path is big enough */
    checkPath(value);

    fprintf(stdout, "|setPath\n"); /* debug */
    fprintf(stdout, "|-Path: %s\n", path); /* debug */
    fprintf(stdout, "|-Path length: %ld\n", path_length); /* debug */
    fprintf(stdout, "|-Strlen(path): %ld\n", strlen(path)); /* debug */

    if (concatenate) {
        /* the given string is concatenated with the existing path */
        strncat(path, value, strlen(value));

        fprintf(stdout, "|-Concatenate: path after: %s\n", path); /* debug */
        fprintf(stdout, "|-Concatenate: strlen(path): %ld\n", strlen(path)); /* debug */
        fprintf(stdout, "|-------------------------------------------------\n"); /* debug */

    } else {
        /* overwrite path with given string */
        snprintf(path, strlen(value) + 1, "%s", value);

        fprintf(stdout, "|-Overwritten: path is overwritten\n"); /* debug */
        fprintf(stdout, "|-Overwritten: new path: %s\n", path); /* debug */
        fprintf(stdout, "|-Overwritten: strlen(path): %ld\n", strlen(path)); /* debug */
        fprintf(stdout, "|-------------------------------------------------\n"); /* debug */
    }
}
