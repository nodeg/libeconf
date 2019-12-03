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
static bool fileExist(const char *);
static void newProcess(const char *, char *, const char *, econf_file *);

static const char *TMPPATH= "/tmp";
static const char *TMPFILE_1 = "econfctl.tmp";
static const char *TMPFILE_2 = "econfctl_merged.tmp";
static bool isRoot = false;

int main (int argc, char *argv[]) {
    fprintf(stdout, "\n|------------------DEBUG Messages------------------| \n"); /* debug */
    /* only do something if we have an input */
    if (argc < 3) {
        usage("Missing command!\n");
    }

    econf_file *key_file = NULL;
    econf_err error;

    // int argv2Compare = (strcmp(argv[2], "--full") == 0 || strcmp(argv[2], "--force") == 0); /* debug */
    // fprintf(stdout, "argv2Compare: %d\n", argv2Compare); /* debug */


    // fprintf(stdout, "|-argc: %d\n", argc); /* debug */
    // fprintf(stdout, "|-argv[0]: %s\n", argv[0]); /* debug */
    // fprintf(stdout, "|-argv[1]: %s\n", argv[1]); /* debug */
    // fprintf(stdout, "|-argv[2]: %s\n", argv[2]); /* debug */
    if (argc == 3 && (strcmp(argv[2], "--full") == 0 || strcmp(argv[2], "--force") == 0)) {
        usage("Missing filename force/full!\n");
        exit(EXIT_FAILURE);
    }

    char fileName[4096];
    memset(fileName, 0, 4096);
    char *suffix = NULL;
    char *posLastDot;

    /* get position of the last dot in the filename to extract
     * the suffix from it.
     * using edit we have to include the possibility that someone
     * uses the --force or --full argument and take that into
     * account when extracting the suffix.
     */
    if (argc == 3) {
        posLastDot = strrchr(argv[2], 46); /* . in ASCII is 46 */
    } else {
        posLastDot = strrchr(argv[3], 46); /* . in ASCII is 46 */
    }

    // fprintf(stdout, "length argv[2]: %ld\n", strlen(argv[2])); /* debug */
    // fprintf(stdout, "argv[2]: %s\n", argv[2]); /* debug */

    if (posLastDot == NULL) {
        usage("Currently only working with a dot in the filename!\n");
        exit(EXIT_FAILURE);
    }
    // fprintf(stdout, "length posLastDot: %ld\n", strlen(posLastDot)); /* debug */

    suffix = posLastDot;

    // fprintf(stdout, "length suffix: %ld\n", strlen(suffix)); /* debug */
    // fprintf(stdout, "Suffix: %s\n", suffix); /* debug */

    if (argc == 3) {
        snprintf(fileName, strlen(argv[2]) -  strlen(posLastDot) + 1, "%s", argv[2]);
    } else {
        snprintf(fileName, strlen(argv[3]) -  strlen(posLastDot) + 1, "%s", argv[3]);
    }

    // fprintf(stdout, "length filename: %ld\n", strlen(fileName)); /* debug */
    // fprintf(stdout, "fileName: %s\n", fileName); /* debug */

    /**
     * econf_err econf_readDirs(econf_file **key_file,
     *				const char *usr_conf_dir,
     *				const char *etc_conf_dir,
     *				const char *project_name,
     *				const char *config_suffix,
     *				const char *delimiter,
     *				const char *comment);
     *
     * @brief Read the specified config file in /etc and /usr.
     * @param key_file The econf_file struct.
     * @param usr_conf_dir The /usr path of the config file.
     * @param etc_conf_dir The /etc path of the config file.
     * @param project_name The file name itself.
     * @param config_suffix The file extension.
     * @param delimiter The delimiter used in the config file.
     * @param comment The comment character used in the config file.
     *
     */
    fprintf(stdout, "|-Filling key_file\n");
    /* static values at the moment */
    if ((error = econf_readDirs(&key_file, "/usr/etc", "/etc", fileName, suffix,"=", "#")))
    {
        fprintf(stderr, "%s\n", econf_errString(error));
        econf_free(key_file);
        return EXIT_FAILURE;
    }

    /**
     * @brief This command will read all snippets for example.conf
     *        (econf_readDirs) and print all groups, keys and their
     *        values as an application would see them.
     */
    // } else
    if (strcmp(argv[1], "show") == 0) {
        if (argc < 3) {
            usage("Missing filename!\n");
        }
        if (argc >= 4) {
            usage("Too many arguments!\n");
        }

        /* some loops for retrieving the content from an econf key_file */
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
                if ((error = econf_getStringValue(key_file, groups[g], keys[k], &value))
                    || value == NULL || strlen(value) == 0) {
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
     *          If the user has super user rights, the file will be created in
     *          /etc/file.d/. Otherwise the file will be created in XDG_CONF_HOME,
     *          which is normally $HOME/.config/.
     *
     *  TODO:
     *      - Replace static values (path, dir) with libeconf API
     */
    } else if (strcmp(argv[1], "edit") == 0) {
            // fprintf(stdout, "|-argc: %d\n", argc); /* debug */
            // fprintf(stdout, "|-argv[0]: %s\n", argv[0]); /* debug */
            // fprintf(stdout, "|-argv[1]: %s\n", argv[1]); /* debug */
            // fprintf(stdout, "|-argv[2]: %s\n", argv[2]); /* debug */
            // fprintf(stdout, "|-argv[3]: %s\n", argv[3]); /* debug */
        if (argc < 3) {
            usage("Missing filename!\n");
            exit(EXIT_FAILURE);
        } else if (argc > 4) {
            usage("Too many arguments!\n");
            exit(EXIT_FAILURE);
        } else if (argc == 4 && (strcmp(argv[2], "--force") != 0) && (strcmp(argv[2], "--full") != 0)) {
            usage("Unknown command!\n");
            exit(EXIT_FAILURE);
        } else if (argc == 3 && ((strcmp(argv[2], "--force") == 0) || (strcmp(argv[2], "--full") == 0))) {
                usage("Missing filename!\n");
                exit(EXIT_FAILURE);
        } else {
            /* set filename to the proper argv argument */
            //char *filename = NULL;
            if (argc == 3) {
                snprintf(fileName, strlen(argv[2]) + 1, "%s", argv[2]);
                //fileName = argv[2];
            } else if (argc == 4) {
                snprintf(fileName, strlen(argv[3]) + 1, "%s", argv[3]);
                //fileName = argv[3];
            }

            static const char CONFDIR[] = "/.config";
            char path[4096];
            char home[4096];
            memset(path, 0, 4096);
            memset(home, 0, 4096);
            snprintf(home, strlen(getenv("HOME")) + 1, "%s", getenv("HOME"));
            bool fileExists = false;
            // bool isRoot = false;
            uid_t uid = getuid();
            uid_t euid = geteuid();

            /* set path to given string */
            snprintf(path, strlen("/etc") + 1, "%s", "/etc");
            fprintf(stdout, "|-Path: %s\n", path); /* debug */

            const char *editor = getenv("EDITOR");
            if(editor == NULL) {
                /* if no editor is specified take vim as default */
                editor = "/usr/bin/vim";
            }
            char *xdgConfigDir = getenv("XDG_CONFIG_HOME");
            if (xdgConfigDir == NULL) {
                /* if no XDG_CONFIG_HOME ist specified take ~/.config as
                 * default
                 */
                strncat(home, CONFDIR, sizeof(home) - strlen(home) - 1);
                // fprintf(stdout, "|-Home: %s\n", home); /* debug */
                xdgConfigDir = home;
                fprintf(stdout, "|-XDG conf dir: %s\n", xdgConfigDir); /* debug */
            } else {
                /* XDG_CONFIG_HOME does not end with an '/', we have to add one manually */
                /* TODO: needs to be fixed */
                //strncat(home, "/", sizeof(home) - strlen(home) - 1);
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
            if (argc == 4 && strcmp(argv[2], "--full") == 0) {
                fprintf(stdout, "|command: edit --full %s --> TODO\n", fileName); /* debug */
                /* TODO
                *  - check if config file exists; probably with libeconf
                *  - check for roor permissions
                *  - copy file to /etc
                *  - open file with $EDITOR
                */

            /* if the config file does -not- exist, create it */
            } else if (argc == 4 && strcmp(argv[2], "--force") == 0) {
                fprintf(stdout, "|command: edit --force %s\n", fileName); /* debug */

                if (!fileExists) {
                    fprintf(stdout, "|--File does not exist in /etc\n"); /* debug */

                    if (isRoot) {
                        fprintf(stdout, "|--Root path\n"); /* debug */
                        /* check if file.d directory already exists and create
                         * new one if it does not.
                         */
                         /* construct directory name */
                        // fprintf(stdout, "|--strncat()--\n"); /* debug */
                        strncat(path, ".d/", sizeof(path) - strlen(path) - 1);
                        fprintf(stdout, "|--Path: %s\n", path); /* debug */

                        /* check if filename.d directory already exist */
                        if (access(path, F_OK) != 0) {
                            if (errno == ENOENT) {
                                /* directory does not exist */
                                fprintf(stdout, "|--Directory does not exist\n"); /* debug */

                                /* create parent directory (filename.d) */
                                int mkDir = mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); // 755
                                if (mkDir != 0) {
                                    fprintf(stderr, "Error with mkdir()!\n");
                                    exit(EXIT_FAILURE);
                                }
                                /* apply filename to path */
                                // fprintf(stdout, "|--strncat()--\n"); /* debug */
                                strncat(path, fileName, sizeof(path) - strlen(path) - 1);
                                fprintf(stdout, "|--Path: %s\n", path); /* debug */
                                /* open EDITOR in child process */
                                newProcess(editor, path, fileName, key_file);

                                /* if no file was saved, delete newly created directory */
                                /* just a copy of path to work with rmdir() */
                                char tmpPath[4096];
                                memset(tmpPath, 0, 4096);
                                memmove(tmpPath, path, strlen(path));
                                // fprintf(stdout, "|--strlen(path) before rmdir(): %ld\n", strlen(path)); /* debug */
                                // fprintf(stdout, "|--strlen(filename) before rmdir(): %ld\n", strlen(filename)); /* debug */
                                // fprintf(stdout, "|--tmpPath before rmdir(): %s\n", tmpPath); /* debug */
                                memset(path, 0, 4096);
                                memmove(path, tmpPath, strlen(tmpPath) - strlen(fileName));
                                fprintf(stdout, "|--Path before rmdir(): %s\n", path); /* debug */
                                rmdir(path);
                                /* no error checking for rmdir()! */

                            }
                        } else { /* filename.d directory does already exist */
                            fprintf(stdout, "|--Directory already exists\n"); /* debug */
                            /* apply filename to path */
                            // fprintf(stdout, "|--strncat()--\n"); /* debug */
                            strncat(path, fileName, sizeof(path) - strlen(path) - 1);
                            fprintf(stdout, "|--Path: %s\n", path); /* debug */
                            /* Open $EDITOR in new process */
                            newProcess(editor, path, fileName, key_file);
                        }
                    } else { /* not root */
                        /* the user is not root and therefore the path has to
                         * be adjusted, since then the file is saved in the HOME
                         * directory of the user.
                         */
                        fprintf(stdout, "|--Not root\n"); /* debug */
                        fprintf(stdout, "|--Overwriting path with XDG_CONF_DIR\n\n"); /* debug */
                        /* overwrite path with xdgConfigDir */
                        snprintf(path, strlen(xdgConfigDir) + 1, "%s", xdgConfigDir);
                        fprintf(stdout, "|--Path: %s\n", path); /* debug */
                        // /* concatenate path and filename  */
                        // fprintf(stdout, "|--strncat()--\n"); /* debug */
                        // strncat(path, fileName, sizeof(path) - strlen(path) - 1);
                        fprintf(stdout, "|-Path: %s\n", path); /* debug */
                        // fprintf(stdout, "|-Strlen(path): %ld\n", strlen(path)); /* debug */


                        /* Open $EDITOR in new process */
                        newProcess(editor, path, fileName, key_file);
                    }
                } else {
                    /* the file does already exist. Set new path
                     */
                    fprintf(stdout, "|--File already exists\n"); /* debug */
                    snprintf(path, strlen(xdgConfigDir) + 1, "%s", xdgConfigDir);
                    fprintf(stdout, "|--Path: %s\n", path); /* debug */

                    newProcess(editor, path, fileName, key_file);
                }

            } else if (argc == 3 && ((strcmp(argv[2], "--force") != 0) || (strcmp(argv[2], "--full") != 0))) {
                /* just open $EDITOR and let it handle the file */

                if (isRoot) {
                    /* Open $EDITOR in new process */
                    fprintf(stdout, "|-> Normal path, root\n"); /* debug */
                    fprintf(stdout, "|-Path: %s\n\n", path); /* debug */
                    newProcess(editor, path, fileName, key_file);
                } else {
                    /* the user is not root and therefore the path has to
                     * be adjusted, since then the file is saved in the HOME
                     * directory of the user.
                     */
                    fprintf(stdout, "|-> Normal path, not root\n"); /* debug */

                    /* overwrite path with xdgConfigDir */
                    snprintf(path, strlen(xdgConfigDir) + 1, "%s", xdgConfigDir);
                    fprintf(stdout, "|-Overwriting path with XDG_CONF_DIR: %s\n\n", path); /* debug */
                    // fprintf(stdout, "|--Path: %s\n", path); /* debug */
                    // /* concatenate path with filename  */
                    // fprintf(stdout, "|--strncat()--\n"); /* debug */
                    //strncat(path, filename, sizeof(path) - strlen(path) - 1);
                    // fprintf(stdout, "|-Path: %s\n", path); /* debug */
                    // fprintf(stdout, "|-Strlen(path): %ld\n", strlen(path)); /* debug */

                    /* Open $EDITOR in new process */
                    newProcess(editor, path, fileName, key_file);
                }
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

    /* cleanup */
    econf_free(key_file);

    /* delete tmp files after operation was successfully */
    size_t combined_length = strlen(TMPPATH) + strlen(TMPFILE_2) + 2;
    char tmpFileOne[combined_length];
    memset(tmpFileOne, 0, combined_length);
    snprintf(tmpFileOne, combined_length, "%s%s%s", TMPPATH, "/", TMPFILE_2);
    remove(tmpFileOne);
    //perror("|-Cleanup: remove()"); /* debug */
    combined_length = strlen(TMPPATH) + strlen(TMPFILE_1) + 2;
    char tmpFileTwo[combined_length];
    memset(tmpFileTwo, 0, combined_length);
    snprintf(tmpFileTwo, combined_length, "%s%s%s", TMPPATH, "/", TMPFILE_1);
    remove(tmpFileTwo);
    //perror("|-Cleanup: remove()"); /* debug */
    return EXIT_SUCCESS;
}

/**
 * @brief Print error messages and show the usage.
 */
static void usage(const char *message) {
    fprintf(stderr,"%s\n", message);
    fprintf(stderr, "Usage: econfctl [ COMMANDS ] filename.conf\n\n"
        "COMMANDS:\n"
        "show     reads all snippets for filename.conf and prints all groups,\n"   \
        "         keys and their values.\n"
        "cat      prints the content and the name of the file in the order as\n"   \
        "         read by libeconf.\n"
        "edit     starts the editor EDITOR (environment variable) where the\n"     \
        "         groups, keys and values can be modified and saved afterwards.\n"
        "   --full:   copy the original configuration file to /etc instead of\n"   \
        "             creating drop-in files.\n"
        "   --force:  if the configuration file does not exist, create a new\n"    \
        "             one.\n"
        "revert   reverts all changes to the vendor versions. Basically deletes\n" \
        "         the config file in /etc.\n\n");
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
 * @brief Checks if a directory or parts of the path already exists
 * @param dir The path/directory to check
 * @return true if the file already exists
 *         false if the file does not exist
 */
static bool dirExists(const char *dir) {
    if (access(dir, F_OK) == -1 && errno == ENOENT) {
            return false;
    } else if (access(dir, F_OK) == 0) {
        return true;
    }
    return false;
}

/**
 * @brief Creates a new process to execute a command.
 * @param     command The command which should be executed
 * @param     path The constructed path for saving the file later
 * @param     key_file the stored config file information
 *
 * TODO: - make a diff of the two temp files to see what actually changed and
 *         write only that change e.g. into a drop-in file.
 */
static void newProcess(const char *command, char *path, const char *filename, econf_file *key_file) {
    fprintf(stdout, "\n|----Starting fork()----\n"); /* debug */
    fprintf(stdout, "|-command: %s\n", command); /* debug */
    fprintf(stdout, "|-path: %s\n", path); /* debug */
    fprintf(stdout, "|-filename: %s\n", filename); /* debug */

    econf_err error;
    int wstatus = 0;
    pid_t pid = fork();

    if (pid == -1) {
        fprintf(stderr, "Error with fork()\n");
        exit(EXIT_FAILURE);

    } else if (pid == 0) { /* child */

        /* write contents of the key_file to 2 temporary files */
        // fprintf(stdout, "|-Child: econf_writeFile() TMPFILE_1\n"); /* debug */
        if ((error = econf_writeFile(key_file, TMPPATH, TMPFILE_1)))
        {
            fprintf(stdout, "|-Child: econf_writeFile() 1 Error!\n"); /* debug */
            fprintf(stderr, "%s\n", econf_errString(error));
            econf_free(key_file);
            exit(EXIT_FAILURE);
        }
        // fprintf(stdout, "|-Child: econf_writeFile() TMPFILE_2\n"); /* debug */
        if ((error = econf_writeFile(key_file, TMPPATH, TMPFILE_2)))
        {
            fprintf(stdout, "|-Child: econf_writeFile() 2  Error!\n"); /* debug */
            fprintf(stderr, "%s\n", econf_errString(error));
            econf_free(key_file);
            exit(EXIT_FAILURE);
        }

        /*
         * combine path and filename of the tmp file and set permission to 600
         * for both
         */
        size_t combined_length = strlen(TMPPATH) + strlen(TMPFILE_1) + 2;
        char combined_tmp1[combined_length];
        memset(combined_tmp1, 0, combined_length);
        snprintf(combined_tmp1, combined_length, "%s%s%s", TMPPATH, "/", TMPFILE_1);

        int perm = chmod(combined_tmp1, S_IRUSR | S_IWUSR);
        if (perm != 0) {
            exit(EXIT_FAILURE);
        }

        combined_length = strlen(TMPPATH) + strlen(TMPFILE_2) + 2;
        char combined_tmp2[combined_length];
        memset(combined_tmp2, 0, combined_length);
        snprintf(combined_tmp2, combined_length, "%s%s%s", TMPPATH, "/", TMPFILE_2);

        perm = chmod(combined_tmp2, S_IRUSR | S_IWUSR);
        if (perm != 0) {
            exit(EXIT_FAILURE);
        }
        // fprintf(stdout, "|-Child: path combined_tmp1: %s\n", combined_tmp1); /* debug */
        // fprintf(stdout, "|-Child: path combined_tmp2: %s\n", combined_tmp2); /* debug */

        execlp(command, command, combined_tmp2, NULL);

    } else { /* parent */
        if (waitpid(pid, &wstatus, 0) == - 1) {
            fprintf(stderr, "Error using waitpid().\n");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(wstatus)) {
            fprintf(stdout, "|-Exitstatus child (0 = OK): %d\n\n", WEXITSTATUS(wstatus));
        }

        /* saved edits in new key_file */
        econf_file *key_file_after = NULL;
        size_t combined_length = strlen(TMPPATH) + strlen(TMPFILE_2) + 2;
        char tmpFile[combined_length];
        memset(tmpFile, 0, combined_length);
        snprintf(tmpFile, combined_length, "%s%s%s", TMPPATH, "/", TMPFILE_2);

        if ((error = econf_readFile(&key_file_after, tmpFile, "=", "#")))
        {
            fprintf(stdout, "econf_readFile() 3 Error!\n"); /* debug */
            fprintf(stderr, "%s\n", econf_errString(error));
            econf_free(key_file);
            econf_free(key_file_after);
            exit(EXIT_FAILURE);
        }

        if (isRoot) {
            /* only ask root for the prefered location to save the edits */
            char input[2] = "";
            do {
                fprintf(stdout, "Save as drop-in file in /etc/filename.d?\nyes [y], no [n]\n");
                fgets(input, 2, stdin);
            } while (strcmp(input, "y") != 0 && strcmp(input, "n") != 0);

            /* copy files where they belong to, using path */
            char savePath[4096];
            memset(savePath, 0, 4096);

            if (strcmp(input, "y") == 0) {
                snprintf(savePath, strlen(path) + strlen(filename) + 4, "%s%s%s%s", path, "/", filename, ".d");

                fprintf(stdout, "savePath: %s\n", savePath);  /* debug */
                fprintf(stdout, "filename: %s\n", filename);  /* debug */
                fprintf(stdout, "dirExists() (1 = Yes): %d\n", dirExists(savePath));  /* debug */

                /* check if directory exists and create it if necessary */
                if (!dirExists(savePath)) {
                    /* create parent directory (filename.d) */
                    fprintf(stdout, "create parent directory\n");  /* debug */
                    int mkDir = mkdir(savePath, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP); // 750
                    if (mkDir != 0) {
                        fprintf(stderr, "Error with mkdir()!\n");
                        econf_free(key_file);
                        exit(EXIT_FAILURE);
                    }
                }

                if ((error = econf_writeFile(key_file_after, savePath, filename)))
                {
                    fprintf(stdout, "Save as drop-in file: econf_writeFile() 4 Error!\n"); /* debug */
                    fprintf(stderr, "%s\n", econf_errString(error));
                    econf_free(key_file);
                    econf_free(key_file_after);
                    exit(EXIT_FAILURE);
                }
            } else { // do not save as drop in file
                snprintf(savePath, strlen(path) + 2, "%s%s", path, "/");
                if ((error = econf_writeFile(key_file_after, savePath, filename)))
                {
                    fprintf(stdout, "Save normally in /etc: econf_writeFile() 5 Error!\n"); /* debug */
                    fprintf(stderr, "%s\n", econf_errString(error));
                    econf_free(key_file);
                    econf_free(key_file_after);
                    exit(EXIT_FAILURE);
                }

            }
        } /* not root */
        fprintf(stdout, "Save normally in xdgConfigDir\n"); /* debug */
        if ((error = econf_writeFile(key_file_after, path, filename)))
        {
            fprintf(stdout, "Save normally in xdgConfigDir: econf_writeFile() 5 Error!\n"); /* debug */
            fprintf(stderr, "%s\n", econf_errString(error));
            econf_free(key_file);
            econf_free(key_file_after);
            exit(EXIT_FAILURE);
        }
        /* cleanup */
        econf_free(key_file_after);
    }
}
