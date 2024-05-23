/*
 * Copyright (c) 2011, 2012 Kyle Isom <kyle@tyrfingr.is>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>


#define DEFAULT_PASSES  3
#define DEV_RANDOM      "/dev/urandom"
#define MAX_CHUNK       4096


static int       do_wipe(char *, size_t, int);
static int       wipe(char *);
static int       rmdirs(const char *, size_t);
static void      usage(void);
static void      version(void);

extern char     *__progname;


/*
 * overwrite a file with one or more passes of random data, then unlink it.
 */
int
main(int argc, char **argv)
{
        size_t passes, tmp_passes, wiped, failed, i;
        char **file_ptr, **wipe_success, **wipe_fail;
        int retval, opt, verbose, recur;

        passes  = DEFAULT_PASSES;
        retval  = EX_DATAERR;
        wiped   = 0;
        failed  = 0;
        verbose = 0;
        recur   = 0;

        if (argc == 1)
                usage();

        while (-1 != (opt = getopt(argc, argv, "hn:rvV"))) {
                switch( opt ) {
                case 'h':
                        usage();
                        break;              /* don't technically need it but meh */
                case 'n':
                        tmp_passes = atoi(optarg);
                        passes = tmp_passes > 0 ? tmp_passes : passes;
                        break;
                case 'r':
                        recur = 1;
                        break;
                case 'v':
                        verbose = 1;
                        break;
                case 'V':
                        version();
                        exit(EX_OK);
                default:
                        usage();
                        /* NOTREACHED */
                }
        }

        argc -= optind;
        argv += optind;

        file_ptr = argv;
        wipe_success = calloc(argc, sizeof(char *));
        wipe_fail    = calloc(argc, sizeof(char *));


        while (NULL != *file_ptr) {
                printf("%s: ", *file_ptr);
                fflush(stdout);

                if (EX_OK != do_wipe(*file_ptr, passes, recur)) {
                        wipe_fail[failed] = strdup(*file_ptr);
                        failed++;
                } else {
                        wipe_success[wiped] = strdup(*file_ptr);
                        wiped++;
                        printf(" OK!");
                }
                file_ptr++;
                printf("\n");
        }

        if (verbose) {
                if (0 < wiped) {
                        printf("success: ");
                        for( i = 0; i < wiped; ++i ) {
                                printf("%s ", wipe_success[i]);
                        }
                        printf("\n");
                }

                if (0 < failed) {
                        printf("failures: ");
                        for( i = 0; i < failed; ++i ) {
                                printf("%s ", wipe_fail[i]);
                        }
                        printf("\n");
                }
        }

        /* free allocated memory */
        for (i = 0; i < failed; ++i) {
                free(wipe_fail[i]);
                wipe_fail[i] = NULL;
        }
        free(wipe_fail);
        wipe_fail = NULL;

        for (i = 0; i < wiped; ++i) {
                free(wipe_success[i]);
                wipe_success[i] = NULL;
        }
        free(wipe_success);
        wipe_success = NULL;

        return retval;
}


/*
 * takes a filename and the number of passes to wipe the file
 */
int
do_wipe(char *filename, size_t passes, int recur)
{
        struct stat sb;
        size_t filesize, i;
        int retval;

        retval = EX_IOERR;
        if (-1 == stat(filename, &sb)) {
                warn("%s", filename);
                return retval;
        }

        if (recur && sb.st_mode & S_IFDIR) {
                if (EX_OK != rmdirs(filename, passes)) {
                        printf("!");
                        return retval;
                } else {
                        printf(".");
                        fflush(stdout);
                        retval = EX_OK;
                }

        } else {
                filesize = sb.st_size;

                for (i = 0; i < passes; ++i) {
                        if (EX_OK != wipe(filename)) {
                                printf("!");
                                return retval;
                        } else if (-1 == stat(filename, &sb)) {
                                printf("!");
                                return retval;
                        } else if (filesize != (size_t)sb.st_size) {
                                printf("!");
                                return retval;
                        }
                }

                if (-1 == truncate(filename, (off_t)0))
                        warn("%s", filename);

                if (-1 == unlink(filename)) {
                        warn("%s", filename);
                } else {
                        printf(".");
                        fflush(stdout);
                        retval = EX_OK;
                }
        }

        return retval;
}


/*
 * takes a filename and attempts to overwrite it with random data
 */
int
wipe(char *filename)
{
        struct stat sb;

        size_t chunk, filesize, rdsz, wiped, wrsz;
        FILE *devrandom, *target;
        int retval;
        char *rdata;

        retval = EX_IOERR;
        wiped  = 0;

        if (-1 == stat(filename, &sb))
                return retval;

        filesize = sb.st_size;

        /*
         * open devrandom first: if this fails, we don't want to touch the
         * target file.
         */
        devrandom = fopen(DEV_RANDOM, "r");
        if (NULL == devrandom || -1 == ferror(devrandom)) {
                warn("failed to open PRNG %s!", DEV_RANDOM);
                return retval;
        }

        /*
         * for security purposes, we want to make sure to actually overwrite
         * the file. r+ gives us read/write but more importantly, sets the
         * write stream at the beginning of the file. a side note is that when
         * overwriting a file, its size will never seem to change.
         */
        target   = fopen(filename, "r+");
        if (NULL == target || -1 == ferror(target)) {
                warn("failed to open %s", filename);
                fclose(devrandom);
                return retval;
        }

        rewind(target);

        /*
         * wait to calloc until we really need the data - makes cleaning up less
         * tricky.
         */
        rdata = calloc(MAX_CHUNK, sizeof(char));
        if (NULL == rdata) {
                warn("could not allocate random data memory");
                fclose(devrandom);
                fclose(target);
                return retval;
        }

        while (wiped < filesize) {
                chunk = filesize - wiped;
                chunk = chunk > MAX_CHUNK ? MAX_CHUNK : chunk;

                rdsz  =  fread( rdata, sizeof(char), chunk, devrandom );
                wrsz  = fwrite( rdata, sizeof(char), chunk, target );

                if (-1 == stat(filename, &sb)) {
                        warn(" stat on %s failed", filename);
                        break;
                }
                if ((rdsz != wrsz) || (filesize != (unsigned int)sb.st_size)) {
                        warn("invalid read/write size");
                        break;
                }

                wiped += chunk;
        }

        if ((0 != fclose(devrandom)) || (0 != fclose(target)))
                warn("%s", filename);
        else
                retval = EX_OK;

        free(rdata);
        rdata = NULL;

        return retval;
}


/*
 * remove a directory, all files in it, and all subdirectories.
 */
int
rmdirs(const char *path, size_t passes)
{
        char             child[FILENAME_MAX + 1];
        struct dirent   *dp;
        DIR             *dirp;
        int              fail = 0;

        if (NULL == (dirp = opendir(path)))
                return EX_IOERR;
        while (NULL != (dp = readdir(dirp))) {
                if (0 == strncmp("..", dp->d_name, 3))
                        continue;
                if (0 == strncmp(".", dp->d_name, 2))
                        continue;
                snprintf(child, FILENAME_MAX, "%s/%s", path, dp->d_name);
                if (DT_DIR == dp->d_type) {
                        fail = rmdirs(child, passes);
                        if (EX_IOERR == fail)
                                break;
                } else {
                        fail = do_wipe(child, passes, 1);
                        if (EX_IOERR == fail)
                                break;
                }
        }
        if (-1 == closedir(dirp))
                return EX_IOERR;
        if (EX_IOERR == fail)
                return EX_IOERR;

        if (-1 == rmdir(path))
                return EX_IOERR;
        else
                return EX_OK;
}


/*
 * print a quick usage message
 */
void
usage(void)
{
        version();
        printf("usage: %s [-v] [-n number] files\n", __progname);
        printf("        -n passes   specify number of passes\n");
        printf("                    (default is %d passes)\n", DEFAULT_PASSES);
        printf("        -r          recursive delete\n");
        printf("        -v          display list of failures and wiped files"
            " after wiping\n"
            "                    (verbose mode).\n");
        printf("        -V          print version information.");

        printf("\n");
        exit(EX_USAGE);
}


/*
 * print program version information
 */
void
version(void)
{
        printf("%s\n", srm_VERSION);
}


