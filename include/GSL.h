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

#ifndef PSOARCHIVE__GSL_H
#define PSOARCHIVE__GSL_H

#include "psoarchive-error.h"

#include <stdint.h>
#include <sys/types.h>

/* Opaque GSL structures. */
struct pso_gsl_read;
typedef struct pso_gsl_read pso_gsl_read_t;

struct pso_gsl_write;
typedef struct pso_gsl_write pso_gsl_write_t;

/* Parameters for the flags parameter for pso_gsl_read_open() and pso_gsl_new()
   functions. These are optional for _open() (the default will automatically try
   to determine the endianness), but one of these are required for _new(). */
#define PSO_GSL_BIG_ENDIAN      (1 << 0)
#define PSO_GSL_LITTLE_ENDIAN   (1 << 1)

/* Archive reading functionality... */
pso_gsl_read_t *pso_gsl_read_open(const char *fn, uint32_t flags,
                                  pso_error_t *err);
pso_gsl_read_t *pso_gsl_read_open_fd(int fd, uint32_t len, uint32_t flags,
                                     pso_error_t *err);
pso_error_t pso_gsl_read_close(pso_gsl_read_t *a);

uint32_t pso_gsl_file_count(pso_gsl_read_t *a);

uint32_t pso_gsl_file_lookup(pso_gsl_read_t *a, const char *fn);
ssize_t pso_gsl_file_size(pso_gsl_read_t *a, uint32_t hnd);
pso_error_t pso_gsl_file_name(pso_gsl_read_t *a, uint32_t hnd, char *fn,
                              size_t len);

ssize_t pso_gsl_file_read(pso_gsl_read_t *a, uint32_t hnd, uint8_t *buf,
                          size_t len);

/* Archive creation/writing functionality... */
pso_gsl_write_t *pso_gsl_new(const char *fn, uint32_t flags, pso_error_t *err);
pso_gsl_write_t *pso_gsl_new_fd(int fd, uint32_t flags, pso_error_t *err);

pso_error_t pso_gsl_write_close(pso_gsl_write_t *a);

/* Set the size of the file table. This is only valid on a newly created write
   structure. If you have already written files to this archive, this call will
   fail with PSOERROR_EFATAL. The ents parameter is rounded to the nearest block
   length (assuming it is above the minimum of 256) and is not rounded to a
   power-of-two, as SEGA's tool seems to do. You MUST call this function before
   writing to the archive if you intend to store more than 256 files in the
   archive! For safety (and compatibility with various tools), you should always
   set this to at least one more than the number of files you want in the
   archive. */
pso_error_t pso_gsl_write_set_ftab_size(pso_gsl_write_t *a, uint32_t ents);

pso_error_t pso_gsl_write_add(pso_gsl_write_t *a, const char *fn,
                              const uint8_t *data, uint32_t len);
pso_error_t pso_gsl_write_add_fd(pso_gsl_write_t *a, const char *fn, int fd,
                                 uint32_t len);
pso_error_t pso_gsl_write_add_file(pso_gsl_write_t *a, const char *afn,
                                   const char *fn);


#endif /* !PSOARCHIVE__GSL_H */
