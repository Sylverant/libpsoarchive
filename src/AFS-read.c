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
#include <inttypes.h>
#endif

#include "AFS.h"

struct afs_file {
    uint32_t offset;
    uint32_t size;
};

struct pso_afs_read {
    int fd;
    struct afs_file *files;

    uint32_t file_count;
    uint32_t flags;
};

pso_afs_read_t *pso_afs_read_open_fd(int fd, uint32_t len, uint32_t flags,
                                     pso_error_t *err) {
    pso_afs_read_t *rv;
    pso_error_t erv = PSOARCHIVE_EFATAL;
    uint32_t i, files;
    uint8_t buf[8];

    /* Read the beginning of the file to make sure it is an AFS archive and to
       get the number of files... */
    if(read(fd, buf, 8) != 8) {
        erv = PSOARCHIVE_NOARCHIVE;
        goto ret_err;
    }

    /* The first 4 bytes must be 'AFS\0' */
    if(buf[0] != 0x41 || buf[1] != 0x46 || buf[2] != 0x53 || buf[3] != 0x00) {
        erv = PSOARCHIVE_NOARCHIVE;
        goto ret_err;
    }

    files = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    if(files > 65535) {
        erv = PSOARCHIVE_EFATAL;
        goto ret_err;
    }

    /* Allocate our archive handle... */
    if(!(rv = (pso_afs_read_t *)malloc(sizeof(pso_afs_read_t)))) {
        erv = PSOARCHIVE_EMEM;
        goto ret_err;
    }

    /* Allocate some file handles... */
    rv->files = (struct afs_file *)malloc(sizeof(struct afs_file) * files);
    if(!rv->files) {
        erv = PSOARCHIVE_EMEM;
        goto ret_handle;
    }

    /* Read each file's metadata in. */
    for(i = 0; i < files; ++i) {
        if(read(fd, buf, 8) != 8) {
            erv = PSOARCHIVE_EIO;
            goto ret_files;
        }

        rv->files[i].offset = buf[0] | (buf[1] << 8) | (buf[2] << 16) |
            (buf[3] << 24);
        rv->files[i].size = buf[4] | (buf[5] << 8) | (buf[6] << 16) |
            (buf[7] << 24);

        /* Make sure it looks sane... */
        if(rv->files[i].offset + rv->files[i].size > len) {
            erv = PSOARCHIVE_ERANGE;
            goto ret_files;
        }
    }

    /* Set the file count in the handle and shrink the files array... */
    rv->fd = fd;
    rv->file_count = files;
    rv->flags = flags;

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

pso_afs_read_t *pso_afs_read_open(const char *fn, uint32_t flags,
                                  pso_error_t *err) {
    int fd;
    off_t total;
    pso_error_t erv = PSOARCHIVE_EFATAL;
    pso_afs_read_t *rv;

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

    if((rv = pso_afs_read_open_fd(fd, (uint32_t)total, flags, err)))
        return rv;

    /* If we get here, the pso_afs_read_open_fd() function encountered an error.
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

pso_error_t pso_afs_read_close(pso_afs_read_t *a) {
    if(!a || a->fd < 0 || !a->files)
        return PSOARCHIVE_EFATAL;

    close(a->fd);
    free(a->files);
    free(a);

    return PSOARCHIVE_OK;
}

uint32_t pso_afs_file_count(pso_afs_read_t *a) {
    if(!a)
        return 0;

    return a->file_count;
}

uint32_t pso_afs_file_lookup(pso_afs_read_t *a, const char *fn) {
    (void)a;
    (void)fn;
    return PSOARCHIVE_HND_INVALID;
}

pso_error_t pso_afs_file_name(pso_afs_read_t *a, uint32_t hnd, char *fn,
                              size_t len) {
    /* Make sure the arguments are sane... */
    if(!a || hnd >= a->file_count)
        return PSOARCHIVE_EFATAL;

#ifndef _WIN32
    snprintf(fn, len, "%05" PRIu32 ".bin", hnd);
#else
    snprintf(fn, len, "%05I32u.bin", hnd);
#endif

    return PSOARCHIVE_OK;
}

ssize_t pso_afs_file_size(pso_afs_read_t *a, uint32_t hnd) {
    /* Make sure the arguments are sane... */
    if(!a || hnd >= a->file_count)
        return -1;

    return (ssize_t)a->files[hnd].size;
}

ssize_t pso_afs_file_read(pso_afs_read_t *a, uint32_t hnd, uint8_t *buf,
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
