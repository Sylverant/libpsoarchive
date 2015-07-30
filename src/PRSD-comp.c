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

/******************************************************************************
    PRSD Compression Library

    PRSD files are effectively just encrypted PRS files with a small header on
    the top defining the decompressed size of the file and the encryption key.
    The encryption employed for this is the same that is used for packets in
    PSO for Dreamcast and PSOPC (as well as the patch server for PSOBB).

    The code in this file ties together PRS compression with the encryption code
    in PRSD-crypt.c to encode a whole PRSD file.
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "PRSD-common.h"
#include "PRSD.h"
#include "PRS.h"

size_t pso_prsd_max_compressed_size(size_t len) {
    return pso_prsd_max_compressed_size(len) + 8;
}

int pso_prsd_archive(const uint8_t *src, uint8_t **dst, size_t src_len,
                     uint32_t key) {
    size_t dl;
    uint8_t *db;
    int rv;
    struct prsd_crypt_cxt ccxt;

    if(!src || !dst)
        return PSOARCHIVE_EFAULT;

    if(!src_len)
        return PSOARCHIVE_EINVAL;

    /* Figure out the length of our "compressed" buffer and allocate it. */
    dl = pso_prsd_max_compressed_size(src_len);
    if(!(db = malloc((dl + 3) & 0xFFFFFFFC)))
        return PSOARCHIVE_EMEM;

    /* Compress the data into the destination buffer (offset for the header). */
    if((rv = pso_prs_archive2(src, db + 8, src_len, dl - 8)) < 0) {
        free(db);
        return rv;
    }

    /* Encrypt the "compressed" data. */
    pso_prsd_crypt_init(&ccxt, key);
    pso_prsd_crypt(&ccxt, db + 8, dl - 8);

    /* Fill in the header. */
    db[0] = (uint8_t)src_len;
    db[1] = (uint8_t)(src_len >> 8);
    db[2] = (uint8_t)(src_len >> 16);
    db[3] = (uint8_t)(src_len >> 24);
    db[4] = (uint8_t)key;
    db[5] = (uint8_t)(key >> 8);
    db[6] = (uint8_t)(key >> 16);
    db[7] = (uint8_t)(key >> 24);

    /* We're done, return the length of the full buffer. */
    *dst = db;
    return rv + 8;
}

int pso_prsd_compress(const uint8_t *src, uint8_t **dst, size_t src_len,
                      uint32_t key) {
    uint8_t *db, *db2;
    int rv;
    struct prsd_crypt_cxt ccxt;

    if(!src || !dst)
        return PSOARCHIVE_EFAULT;

    if(!src_len)
        return PSOARCHIVE_EINVAL;

    /* Ugly... But it'll work...
       Compress the data into a temporary destination buffer. */
    if((rv = pso_prs_compress(src, &db, src_len)) < 0)
        return rv;

    /* Now that we know the full length, allocate space for the whole thing,
       copy the compressed data over to the new buffer, and clean up the other
       one. */
    if(!(db2 = (uint8_t *)malloc((rv + 3) & 0xFFFFFFFC))) {
        free(db);
        return PSOARCHIVE_EMEM;
    }

    memcpy(db2 + 8, db, rv);
    free(db);

    /* Encrypt the compressed data. */
    pso_prsd_crypt_init(&ccxt, key);
    pso_prsd_crypt(&ccxt, db2 + 8, rv);

    /* Fill in the header. */
    db2[0] = (uint8_t)src_len;
    db2[1] = (uint8_t)(src_len >> 8);
    db2[2] = (uint8_t)(src_len >> 16);
    db2[3] = (uint8_t)(src_len >> 24);
    db2[4] = (uint8_t)key;
    db2[5] = (uint8_t)(key >> 8);
    db2[6] = (uint8_t)(key >> 16);
    db2[7] = (uint8_t)(key >> 24);

    /* We're done, return the length of the full buffer. */
    *dst = db2;
    return rv + 8;
}
