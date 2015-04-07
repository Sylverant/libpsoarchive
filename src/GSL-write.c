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

struct pso_gsl_write {
    int fd;

    int ftab_entries;
    int ftab_used;

    uint32_t flags;

    off_t ftab_pos;
    off_t data_pos;
};

static off_t pad_file(int fd, int boundary) {
    off_t pos = lseek(fd, 0, SEEK_CUR);
    uint8_t tmp = 0;

    /* If we aren't actually padding, don't do anything. */
    if(boundary <= 0)
        return pos;

    pos = (pos & ~(boundary - 1)) + boundary;

    if(lseek(fd, pos - 1, SEEK_SET) == (off_t)-1)
        return (off_t)-1;

    if(write(fd, &tmp, 1) != 1)
        return (off_t)-1;

    return pos;
}

pso_gsl_write_t *pso_gsl_new(const char *fn, uint32_t flags, pso_error_t *err) {
    pso_gsl_write_t *rv;
    pso_error_t erv = PSOARCHIVE_OK;

    /* Make sure the user specified an endianness for the file... */
    if(!(flags & GSL_ENDIANNESS) ||
       (flags & GSL_ENDIANNESS) == GSL_ENDIANNESS) {
        erv = PSOARCHIVE_EFATAL;
        goto ret_err;
    }

    /* Allocate space for our write context. */
    if(!(rv = (pso_gsl_write_t *)malloc(sizeof(pso_gsl_write_t)))) {
        erv = PSOARCHIVE_EMEM;
        goto ret_err;
    }

    /* Open the file specified. */
    if((rv->fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
        erv = PSOARCHIVE_EFILE;
        goto ret_mem;
    }

    /* Fill in the base structure with our defaults. */
    rv->ftab_entries = 256;
    rv->ftab_used = 0;
    rv->ftab_pos = 0;
    rv->data_pos = 256 * 48;
    rv->flags = flags;

    /* We're done, return success. */
    if(err)
        *err = PSOARCHIVE_OK;

    return rv;

ret_mem:
    free(rv);
ret_err:
    if(err)
        *err = erv;

    return NULL;
}

pso_gsl_write_t *pso_gsl_new_fd(int fd, uint32_t flags, pso_error_t *err) {
    pso_gsl_write_t *rv;
    pso_error_t erv = PSOARCHIVE_OK;

    /* Make sure the user specified an endianness for the file... */
    if(!(flags & GSL_ENDIANNESS) ||
       (flags & GSL_ENDIANNESS) == GSL_ENDIANNESS) {
        erv = PSOARCHIVE_EFATAL;
        goto ret_err;
    }

    /* Allocate space for our write context. */
    if(!(rv = (pso_gsl_write_t *)malloc(sizeof(pso_gsl_write_t)))) {
        erv = PSOARCHIVE_EMEM;
        goto ret_err;
    }

    /* Fill in the base structure with our data. */
    rv->fd = fd;
    rv->ftab_entries = 256;
    rv->ftab_used = 0;
    rv->ftab_pos = 0;
    rv->data_pos = 256 * 48;
    rv->flags = flags;

    /* We're done, return success. */
    if(err)
        *err = PSOARCHIVE_OK;

    return rv;

ret_mem:
    free(rv);
ret_err:
    if(err)
        *err = erv;

    return NULL;
}

pso_error_t pso_gsl_write_close(pso_gsl_write_t *a) {
    if(!a || a->fd < 0)
        return PSOARCHIVE_EFATAL;

    close(a->fd);
    free(a);

    return PSOARCHIVE_OK;
}

pso_error_t pso_gsl_write_set_ftab_size(pso_gsl_write_t *a, uint32_t ents) {
    if(!a || a->ftab_used)
        return PSOARCHIVE_EFATAL;

    if(ents < 256)
        ents = 256;

    a->ftab_entries = ents;
    a->data_pos = ents * 48;

    /* Make sure to round to an even block length. */
    if((a->data_pos & 0x7FF)) {
        a->data_pos = (a->data_pos + 0x800) & 0xFFFFF800;
        a->ftab_entries = a->data_pos / 48;
    }

    return PSOARCHIVE_OK;
}

pso_error_t pso_gsl_write_add(pso_gsl_write_t *a, const char *fn,
                              const uint8_t *data, uint32_t len) {
    uint8_t buf[48];
    uint32_t tmp;

    if(!a)
        return PSOARCHIVE_EFATAL;

    /* XXXX: Support extending the file table... */
    if(a->ftab_used == a->ftab_entries - 1)
        return PSOARCHIVE_EFATAL;

    /* Go to where we'll be writing into the file table... */
    if(lseek(a->fd, a->ftab_pos, SEEK_SET) == (off_t)-1)
        return PSOARCHIVE_EIO;

    /* Copy the file data into the buffer... */
    strncpy((char *)buf, fn, 32);
    tmp = a->data_pos >> 11;

    if((a->flags & PSO_GSL_BIG_ENDIAN)) {
        buf[32] = (uint8_t)(tmp >> 24);
        buf[33] = (uint8_t)(tmp >> 16);
        buf[34] = (uint8_t)(tmp >> 8);
        buf[35] = (uint8_t)(tmp);
        buf[36] = (uint8_t)(len >> 24);
        buf[37] = (uint8_t)(len >> 16);
        buf[38] = (uint8_t)(len >> 8);
        buf[39] = (uint8_t)(len);
    }
    else {
        buf[32] = (uint8_t)(tmp);
        buf[33] = (uint8_t)(tmp >> 8);
        buf[34] = (uint8_t)(tmp >> 16);
        buf[35] = (uint8_t)(tmp >> 24);
        buf[36] = (uint8_t)(len);
        buf[37] = (uint8_t)(len >> 8);
        buf[38] = (uint8_t)(len >> 16);
        buf[39] = (uint8_t)(len >> 24);
    }

    buf[40] = buf[41] = buf[42] = buf[43] = 0;
    buf[44] = buf[45] = buf[46] = buf[47] = 0;

    /* Write out the header... */
    if(write(a->fd, buf, 48) != 48)
        return PSOARCHIVE_EIO;

    a->ftab_pos += 48;
    ++a->ftab_used;

    /* Seek to where the file data goes... */
    if(lseek(a->fd, a->data_pos, SEEK_SET) == (off_t)-1)
        return PSOARCHIVE_EIO;

    /* Write the file data out. */
    if(write(a->fd, data, len) != (ssize_t)len)
        return PSOARCHIVE_EIO;

    /* Pad the data position out to where the next file will start. */
    a->data_pos = pad_file(a->fd, 2048);

    /* Done. */
    return PSOARCHIVE_OK;
}

pso_error_t pso_gsl_write_add_fd(pso_gsl_write_t *a, const char *fn, int fd,
                                 uint32_t len) {
    uint8_t buf[512];
    uint32_t tmp;
    ssize_t bytes;

    if(!a)
        return PSOARCHIVE_EFATAL;

    /* XXXX: Support extending the file table... */
    if(a->ftab_used == a->ftab_entries - 1)
        return PSOARCHIVE_EFATAL;

    /* Go to where we'll be writing into the file table... */
    if(lseek(a->fd, a->ftab_pos, SEEK_SET) == (off_t)-1)
        return PSOARCHIVE_EIO;

    /* Copy the file data into the buffer... */
    strncpy((char *)buf, fn, 32);
    tmp = a->data_pos >> 11;

    if((a->flags & PSO_GSL_BIG_ENDIAN)) {
        buf[32] = (uint8_t)(tmp >> 24);
        buf[33] = (uint8_t)(tmp >> 16);
        buf[34] = (uint8_t)(tmp >> 8);
        buf[35] = (uint8_t)(tmp);
        buf[36] = (uint8_t)(len >> 24);
        buf[37] = (uint8_t)(len >> 16);
        buf[38] = (uint8_t)(len >> 8);
        buf[39] = (uint8_t)(len);
    }
    else {
        buf[32] = (uint8_t)(tmp);
        buf[33] = (uint8_t)(tmp >> 8);
        buf[34] = (uint8_t)(tmp >> 16);
        buf[35] = (uint8_t)(tmp >> 24);
        buf[36] = (uint8_t)(len);
        buf[37] = (uint8_t)(len >> 8);
        buf[38] = (uint8_t)(len >> 16);
        buf[39] = (uint8_t)(len >> 24);
    }

    buf[40] = buf[41] = buf[42] = buf[43] = 0;
    buf[44] = buf[45] = buf[46] = buf[47] = 0;

    /* Write out the header... */
    if(write(a->fd, buf, 48) != 48)
        return PSOARCHIVE_EIO;

    a->ftab_pos += 48;
    ++a->ftab_used;

    /* Seek to where the file data goes... */
    if(lseek(a->fd, a->data_pos, SEEK_SET) == (off_t)-1)
        return PSOARCHIVE_EIO;

    /* While there's still data to be read from the file, read it into our
       buffer and write it out to the file. */
    while(len) {
        bytes = len > 512 ? 512 : len;

        if(read(fd, buf, bytes) != bytes)
            return PSOARCHIVE_EIO;

        if(write(a->fd, buf, bytes) != bytes)
            return PSOARCHIVE_EIO;

        len -= bytes;
    }

    /* Pad the data position out to where the next file will start. */
    a->data_pos = pad_file(a->fd, 2048);

    /* Done. */
    return PSOARCHIVE_OK;
}

pso_error_t pso_gsl_write_add_file(pso_gsl_write_t *a, const char *afn,
                                   const char *fn) {
    int fd;
    pso_error_t err;
    off_t len;

    /* Open the file. */
    if((fd = open(fn, O_RDONLY)) < 0)
        return PSOARCHIVE_EFILE;

    /* Figure out how long the file is. */
    if((len = lseek(fd, 0, SEEK_END)) == (off_t)-1) {
        close(fd);
        return PSOARCHIVE_EIO;
    }

    if(lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        close(fd);
        return PSOARCHIVE_EIO;
    }

    /* Add it to the archive... */
    err = pso_gsl_write_add_fd(a, afn, fd, (uint32_t)len);

    /* Clean up... */
    close(fd);
    return err;
}
