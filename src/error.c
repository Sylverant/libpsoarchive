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

#include "psoarchive-error.h"

#define NUM_ERRORS 13

/* Error code -> String map. */
static const char *err_map[NUM_ERRORS + 1] = {
    "No error",
    "File error",
    "Memory allocation error",
    "Fatal error",
    "No archive found",
    "Empty archive",
    "I/O error",
    "Out of range",
    "Invalid pointer",
    "Invalid argument",
    "Out of space",
    "Invalid data during parse",
    "Operation not supported",
    /* EOL */
    "Unknown error"
};

const char *pso_strerror(pso_error_t err) {
    int idx = -err;

    if(idx < 0 || idx >= NUM_ERRORS)
        idx = NUM_ERRORS;

    return err_map[idx];
}
