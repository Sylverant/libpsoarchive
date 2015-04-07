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

#include "GSL.h"

#define GSL_ENDIANNESS (PSO_GSL_BIG_ENDIAN | PSO_GSL_LITTLE_ENDIAN)
#define GSL_FILENAME_LEN    32

struct gsl_file {
    char filename[GSL_FILENAME_LEN];
    uint32_t offset;
    uint32_t size;
};
