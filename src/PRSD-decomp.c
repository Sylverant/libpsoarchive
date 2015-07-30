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
    PRSD Decompression Library

    PRSD files are effectively just encrypted PRS files with a small header on
    the top defining the decompressed size of the file and the encryption key.
    The encryption employed for this is the same that is used for packets in
    PSO for Dreamcast and PSOPC (as well as the patch server for PSOBB).

    The code in this file ties together PRS decompression with the decryption
    code in PRSD-crypt.c to decode a whole PRSD file.
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "PRSD-common.h"
#include "PRSD.h"
#include "PRS.h"

int pso_prsd_decompress_file(const char *fn, uint8_t **dst) {
    long len;
    int rv;
    FILE *fp;
    uint8_t buf[8];
    uint32_t key, unc_len;
    uint8_t *cmp_buf;
    struct prsd_crypt_cxt ccxt;

    if(!fn || !dst)
        return PSOARCHIVE_EFAULT;

    if(!(fp = fopen(fn, "rb")))
        return PSOARCHIVE_EFILE;

    /* Figure out the length of the file. */
    if(fseek(fp, 0, SEEK_END)) {
        fclose(fp);
        return PSOARCHIVE_EIO;
    }

    if((len = ftell(fp)) < 0) {
        fclose(fp);
        return PSOARCHIVE_EIO;
    }

    if(fseek(fp, 0, SEEK_SET)) {
        fclose(fp);
        return PSOARCHIVE_EIO;
    }

    /* Every PRSD file has an 8-byte header and at least a minimal length PRS
       compressed/encrypted segment. Thus, the file must at least be 11 bytes
       in length. */
    if(len < 11) {
        fclose(fp);
        return PSOARCHIVE_EBADMSG;
    }

    /* Read in the file header and parse it. */
    if(fread(buf, 1, 8, fp) != 8) {
        fclose(fp);
        return PSOARCHIVE_EIO;
    }

    unc_len = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    key = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    len -= 8;

    /* Allocate space for the compressed/encrypted data. */
    if(!(cmp_buf = (uint8_t *)malloc((len + 3) & 0xFFFFFFFC))) {
        fclose(fp);
        return PSOARCHIVE_EMEM;
    }

    /* Read in the data from the file. */
    if(fread(cmp_buf, 1, len, fp) != len) {
        free(cmp_buf);
        fclose(fp);
        return PSOARCHIVE_EIO;
    }

    /* We're done with the file, close it. */
    fclose(fp);

    /* Decrypt the file data. */
    pso_prsd_crypt_init(&ccxt, key);
    pso_prsd_crypt(&ccxt, cmp_buf, len);

    /* Now that we have the data decrypted, decompress it. */
    if((rv = pso_prs_decompress_buf(cmp_buf, dst, len)) < 0) {
        free(*dst);
        free(cmp_buf);
        *dst = NULL;
        return rv;
    }

    /* Clean up the compressed buffer, we don't need it anymore. */
    free(cmp_buf);

    /* Does the uncompressed size match what we're expecting from the file
       header? */
    if(rv != (int)unc_len) {
        free(*dst);
        *dst = NULL;
        return PSOARCHIVE_EFATAL;
    }

    /* We're done, return the size of the uncompressed data. */
    return rv;
}

int pso_prsd_decompress_buf(const uint8_t *src, uint8_t **dst, size_t src_len) {
    uint32_t key, unc_len;
    uint8_t *cmp_buf;
    struct prsd_crypt_cxt ccxt;
    int rv;

    /* Verify the input parameters. */
    if(!src || !dst)
        return PSOARCHIVE_EFAULT;

    if(src_len < 11)
        return PSOARCHIVE_EBADMSG;

    /* Grab the uncompressed size and key from the source buffer. */
    unc_len = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
    key = src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24);
    src_len -= 8;

    /* Allocate space for the compressed/encrypted data. */
    if(!(cmp_buf = (uint8_t *)malloc((src_len + 3) & 0xFFFFFFFC)))
        return PSOARCHIVE_EMEM;

    /* Copy the data from the source buffer into our temporary one. */
    memcpy(cmp_buf, src + 8, src_len);

    /* Decrypt the file data. */
    pso_prsd_crypt_init(&ccxt, key);
    pso_prsd_crypt(&ccxt, cmp_buf, src_len);

    /* Now that we have the data decrypted, decompress it. */
    if((rv = pso_prs_decompress_buf(cmp_buf, dst, src_len)) < 0) {
        free(*dst);
        free(cmp_buf);
        *dst = NULL;
        return rv;
    }

    /* Clean up the temporary buffer, we don't need it anymore. */
    free(cmp_buf);

    /* Does the uncompressed size match what we're expecting from the file
       header? */
    if(rv != (int)unc_len) {
        free(*dst);
        *dst = NULL;
        return PSOARCHIVE_EFATAL;
    }

    /* We're done, return the size of the uncompressed data. */
    return rv;
}

int pso_prsd_decompress_buf2(const uint8_t *src, uint8_t *dst, size_t src_len,
                             size_t dst_len) {
    uint32_t key, unc_len;
    uint8_t *cmp_buf;
    struct prsd_crypt_cxt ccxt;
    int rv;

    /* Verify the input parameters. */
    if(!src || !dst)
        return PSOARCHIVE_EFAULT;

    if(src_len < 11)
        return PSOARCHIVE_EBADMSG;

    /* Grab the uncompressed size and key from the source buffer. */
    unc_len = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
    key = src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24);
    src_len -= 8;

    /* Make sure the buffer the user gave us is big enough. */
    if(dst_len < unc_len)
        return PSOARCHIVE_ENOSPC;

    /* Allocate space for the compressed/encrypted data. */
    if(!(cmp_buf = (uint8_t *)malloc((src_len + 3) & 0xFFFFFFFC)))
        return PSOARCHIVE_EMEM;

    /* Copy the data from the source buffer into our temporary one. */
    memcpy(cmp_buf, src + 8, src_len);

    /* Decrypt the file data. */
    pso_prsd_crypt_init(&ccxt, key);
    pso_prsd_crypt(&ccxt, cmp_buf, src_len);

    /* Now that we have the data decrypted, decompress it. */
    if((rv = pso_prs_decompress_buf2(cmp_buf, dst, src_len, dst_len)) < 0) {
        free(cmp_buf);
        return rv;
    }

    /* Clean up the temporary buffer, we don't need it anymore. */
    free(cmp_buf);

    /* Does the uncompressed size match what we're expecting from the file
       header? */
    if(rv != (int)unc_len)
        return PSOARCHIVE_EFATAL;

    /* We're done, return the size of the uncompressed data. */
    return rv;
}

int pso_prsd_decompress_size(const uint8_t *src, size_t src_len) {
    /* Verify the input parameters. */
    if(!src)
        return PSOARCHIVE_EFAULT;

    if(src_len < 11)
        return PSOARCHIVE_EBADMSG;

    return (int)(src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24));
}
