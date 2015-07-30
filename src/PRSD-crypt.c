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
    PRSD Encryption/Decryption Library

    PRSD files are effectively just encrypted PRS files with a small header on
    the top defining the decompressed size of the file and the encryption key.
    The encryption employed for this is the same that is used for packets in
    PSO for Dreamcast and PSOPC (as well as the patch server for PSOBB).

    This is a new, clean implementation of the PSO for Dreamcast/PSOPC
    encryption code. While libpsoarchive could very well use the same encryption
    code used by the rest of Sylverant, that would either imply duplicating the
    code in this library or having a dependency on libsylverant. I will not
    make libpsoarchive dependent on libsylverant for (somewhat) obvious reasons.
    I chose to reimplement the encryption code from scratch to hopefully make it
    a little bit easier to follow than the code that Fuzziqer put out (and I use
    in libsylverant). That code is (quite obviously) almost directly the result
    of disassembling Sega's code and decompiling it. This code works
    equivalently, but is (to me, anyway) a bit easier to understand.

    While this code could (in theory) be used outside of the PRSD decopmression
    code, that is not the intent here (hence why the context structure is in a
    non-installed header file).
 ******************************************************************************/

#include "PRSD-common.h"

#if defined(__BIG_ENDIAN__) || defined(WORDS_BIGENDIAN)
#define LE32(x) (((x >> 24) & 0x00FF) | \
                 ((x >>  8) & 0xFF00) | \
                 ((x & 0xFF00) <<  8) | \
                 ((x & 0x00FF) << 24))
#else
#define LE32(x) x
#endif

static void mix_stream(struct prsd_crypt_cxt *cxt) {
    int i;
    uint32_t *ptr;

    for(i = 24, ptr = cxt->stream + 1; i; --i, ++ptr) {
        *ptr -= *(ptr + 31);
    }

    for(i = 31, ptr = cxt->stream + 25; i; --i, ++ptr) {
        *ptr -= *(ptr - 24);
    }
}

void pso_prsd_crypt_init(struct prsd_crypt_cxt *cxt, uint32_t key) {
    uint32_t i, idx, tmp = 1;

    cxt->stream[55] = key;
    cxt->key = key;

    for(i = 0x15; i <= 0x46E; i += 0x15) {
        idx = i % 55;
        key -= tmp;
        cxt->stream[idx] = tmp;
        tmp = key;
        key = cxt->stream[idx];
    }

    mix_stream(cxt);
    mix_stream(cxt);
    mix_stream(cxt);
    mix_stream(cxt);

    cxt->pos = 56;
}

static inline uint32_t crypt_dword(struct prsd_crypt_cxt *cxt, uint32_t data) {
    if(cxt->pos == 56) {
        mix_stream(cxt);
        cxt->pos = 1;
    }

    return data ^ cxt->stream[cxt->pos++];
}

void pso_prsd_crypt(struct prsd_crypt_cxt *cxt, void *d, uint32_t len) {
    uint32_t *data = (uint32_t *)d;
    uint32_t tmp;

    /* Round the size of the buffer to the next 4-byte boundary. */
    len = (len + 3) & 0xFFFFFFFC;

    while(len > 0) {
        tmp = crypt_dword(cxt, LE32((*data)));
        *data++ = LE32(tmp);
        len -= 4;
    }
}
