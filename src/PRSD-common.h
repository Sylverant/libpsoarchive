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

#include <stdint.h>

struct prsd_crypt_cxt {
    uint32_t stream[56];
    uint32_t key;
    uint32_t pos;
};

/* These functions are all for internal use only. */
void pso_prsd_crypt_init(struct prsd_crypt_cxt *cxt, uint32_t key);
void pso_prsd_crypt(struct prsd_crypt_cxt *cxt, void *d, uint32_t len);
