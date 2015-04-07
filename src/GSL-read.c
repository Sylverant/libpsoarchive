/*
    This file is part of libpsoarchive.

    Copyright (C) 2015 Lawrence Sebald

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 2.1 or
    version 3 of the License.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "GSL-common.h"

struct pso_gsl_read {
    int fd;
    struct gsl_file *files;

    uint32_t file_count;
    uint32_t flags;
};

pso_gsl_read_t *pso_gsl_read_open_fd(int fd, uint32_t len, uint32_t flags,
                                     pso_error_t *err) {
    pso_gsl_read_t *rv;
    pso_error_t erv = PSOARCHIVE_EFATAL;
    uint32_t i, allocd = 256, offset, size, maxfiles;
    uint8_t buf[48];
    void *tmp;

    /* Allocate our archive handle... */
    if(!(rv = (pso_gsl_read_t *)malloc(sizeof(pso_gsl_read_t)))) {
        erv = PSOARCHIVE_EMEM;
        goto ret_err;
    }

    /* Allocate some file handles... */
    rv->files = (struct gsl_file *)malloc(sizeof(struct gsl_file) * 256);
    if(!rv->files) {
        erv = PSOARCHIVE_EMEM;
        goto ret_handle;
    }

    /* Read the first header in... */
    if(read(fd, buf, 48) != 48) {
        erv = PSOARCHIVE_NOARCHIVE;
        goto ret_files;
    }

    /* Make sure there's at least one file... */
    if(buf[0] == 0) {
        erv = PSOARCHIVE_EMPTY;
        goto ret_files;
    }

    /* If the user hasn't specified the endianness, then try to guess. */
    if(!(flags & GSL_ENDIANNESS)) {
        /* Guess big endian first. */
        offset = (buf[35]) | (buf[34] << 8) | (buf[33] << 16) | (buf[32] << 24);
        size = (buf[39]) | (buf[38] << 8) | ( buf[37] << 16) | (buf[36] << 24);

        flags |= PSO_GSL_BIG_ENDIAN;

        /* If the offset of the file is outside of the archive length, the
           we probably guessed wrong, try as little endian. */
        if(offset > len || offset * 2048 > len || size > len) {
            offset = (buf[35] << 24) | (buf[34] << 16) | (buf[33] << 8) |
                (buf[32]);
            size = (buf[39] << 24) | (buf[38] << 16) | ( buf[37] << 8) |
                (buf[36]);
            flags |= PSO_GSL_LITTLE_ENDIAN;

            if(offset * 2048 > len || size > len) {
                erv = PSOARCHIVE_ERANGE;
                goto ret_files;
            }
        }
    }
    else if((flags & PSO_GSL_BIG_ENDIAN)) {
        offset = (buf[35]) | (buf[34] << 8) | (buf[33] << 16) | (buf[32] << 24);
        size = (buf[39]) | (buf[38] << 8) | ( buf[37] << 16) | (buf[36] << 24);
    }
    else /* if((flags & PSO_GSL_LITTLE_ENDIAN)) */ {
        offset = (buf[35] << 24) | (buf[34] << 16) | (buf[33] << 8) | (buf[32]);
        size = (buf[39] << 24) | (buf[38] << 16) | ( buf[37] << 8) | (buf[36]);
    }

    memcpy(rv->files[0].filename, buf, 32);
    rv->files[0].offset = offset * 2048;
    rv->files[0].size = size;
    maxfiles = rv->files[0].offset / 48;

    /* Read the headers for each file... */
    for(i = 1; i < maxfiles; ++i) {
        /* Read in the header... */
        if(read(fd, buf, 48) != 48) {
            erv = PSOARCHIVE_EIO;
            goto ret_files;
        }

        /* Did we hit the end of the file list? */
        if(buf[0] == 0)
            break;

        /* Parse out the size/offset. */
        if((flags & PSO_GSL_BIG_ENDIAN)) {
            offset = (buf[35]) | (buf[34] << 8) | (buf[33] << 16) |
                (buf[32] << 24);
            size = (buf[39]) | (buf[38] << 8) | ( buf[37] << 16) |
                (buf[36] << 24);
        }
        else /* if((flags & PSO_GSL_LITTLE_ENDIAN)) */ {
            offset = (buf[35] << 24) | (buf[34] << 16) | (buf[33] << 8) |
                (buf[32]);
            size = (buf[39] << 24) | (buf[38] << 16) | ( buf[37] << 8) |
                (buf[36]);
        }

        /* Make sure we have space in the array... */
        if(i == allocd) {
            allocd *= 2;

            if(!(tmp = realloc(rv->files, allocd * sizeof(struct gsl_file)))) {
                erv = PSOARCHIVE_EMEM;
                goto ret_files;
            }

            rv->files = (struct gsl_file *)tmp;
        }

        memcpy(rv->files[i].filename, buf, 32);
        rv->files[i].offset = offset * 2048;
        rv->files[i].size = size;

        /* Sanity check... */
        if(rv->files[i].offset + rv->files[i].size > len) {
            erv = PSOARCHIVE_ERANGE;
            goto ret_files;
        }
    }

    /* Set the file count in the handle and shrink the files array... */
    rv->fd = fd;
    rv->file_count = i;
    rv->flags = flags;

    if(i != allocd) {
        /* Don't fail if we can't do the realloc... It'll just waste a bit of
           memory... Won't really hurt anything, though... */
        if((tmp = realloc(rv->files, i * sizeof(struct gsl_file))))
            rv->files = (struct gsl_file *)tmp;

    }

    /* We're done, return... */
    if(err)
        *err = PSOARCHIVE_OK;

    return rv;

ret_files:
    free(rv->files);
ret_handle:
    free(rv);
ret_err:
    if(err)
        *err = erv;

    return NULL;
}

pso_gsl_read_t *pso_gsl_read_open(const char *fn, uint32_t flags,
                                  pso_error_t *err) {
    int fd;
    off_t total;
    pso_error_t erv = PSOARCHIVE_EFATAL;
    pso_gsl_read_t *rv;

    /* Open the file... */
    if(!(fd = open(fn, O_RDONLY))) {
        erv = PSOARCHIVE_EFILE;
        goto ret_err;
    }

    /* Figure out how long the file is. */
    if((total = lseek(fd, 0, SEEK_END)) == (off_t)-1) {
        erv = PSOARCHIVE_EIO;
        goto ret_file;
    }

    if(lseek(fd, 0, SEEK_SET)) {
        erv = PSOARCHIVE_EIO;
        goto ret_file;
    }

    if((rv = pso_gsl_read_open_fd(fd, (uint32_t)total, flags, err)))
        return rv;

    /* If we get here, the pso_gsl_read_open_fd() function encountered an error.
       Clean up the file descriptor and return NULL. The error code is already
       set in err, if applicable. */
    close(fd);
    return NULL;

ret_file:
    close(fd);
ret_err:
    if(err)
        *err = erv;

    return NULL;
}

pso_error_t pso_gsl_read_close(pso_gsl_read_t *a) {
    if(!a || a->fd < 0 || !a->files)
        return PSOARCHIVE_EFATAL;

    close(a->fd);
    free(a->files);
    free(a);

    return PSOARCHIVE_OK;
}

uint32_t pso_gsl_file_count(pso_gsl_read_t *a) {
    if(!a)
        return 0;

    return a->file_count;
}

uint32_t pso_gsl_file_lookup(pso_gsl_read_t *a, const char *fn) {
    uint32_t i;

    /* Make sure we've got sane arguments... */
    if(!a || !fn)
        return PSOARCHIVE_HND_INVALID;

    /* Linear search through the archive for the file we want... */
    for(i = 0; i < a->file_count; ++i) {
        if(!strncmp(fn, a->files[i].filename, 32))
            return i;
    }

    /* Didn't find it, bail out. */
    return PSOARCHIVE_HND_INVALID;
}

pso_error_t pso_gsl_file_name(pso_gsl_read_t *a, uint32_t hnd, char *fn,
                              size_t len) {
    /* Make sure the arguments are sane... */
    if(!a || hnd >= a->file_count)
        return PSOARCHIVE_EFATAL;

    strncpy(fn, a->files[hnd].filename, len);
    return PSOARCHIVE_OK;
}

ssize_t pso_gsl_file_size(pso_gsl_read_t *a, uint32_t hnd) {
    /* Make sure the arguments are sane... */
    if(!a || hnd >= a->file_count)
        return -1;

    return (ssize_t)a->files[hnd].size;
}

ssize_t pso_gsl_file_read(pso_gsl_read_t *a, uint32_t hnd, uint8_t *buf,
                          size_t len) {
    /* Make sure the arguments are sane... */
    if(!a || hnd > a->file_count || !buf || !len)
        return -1;

    /* Seek to the appropriate position in the file. */
    if(lseek(a->fd, a->files[hnd].offset, SEEK_SET) == (off_t) -1)
        return -1;

    /* Figure out how much we're going to read... */
    if(a->files[hnd].size < len)
        len = a->files[hnd].size;

    if(read(a->fd, buf, len) != len)
        return -1;

    return (ssize_t)len;
}
