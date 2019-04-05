/*  Copyright 2013-2014, Gur Stavi, gur.stavi@gmail.com  */

/*
    This file is part of TagLEET.

    TagLEET is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    TagLEET is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TagLEET.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TL_TYPES_H_
#define _TL_TYPES_H_

#include <stdint.h>

namespace TagLEET {

//typedef uint32_t tf_int_t;
typedef uint64_t tf_int_t;

typedef enum {
  TL_ERR_OK = 0,
  TL_ERR_GENERAL,
  TL_ERR_INVALID,
  TL_ERR_NOT_EXIST,
  TL_ERR_NO_MORE,
  TL_ERR_TOO_BIG,
  TL_ERR_ALREADY,
  TL_ERR_BAD_STATE,
  TL_ERR_MEM_ALLOC,
  TL_ERR_ENCODE_CONV,
  TL_ERR_FILE_NOT_EXIST,
  TL_ERR_FILE_TOO_BIG,
  TL_ERR_MODIFIED,
  TL_ERR_SORT,
  TL_ERR_FILE_NOT_OPEN,
  TL_ERR_LINE_TOO_BIG,
  TL_ERR_BUFF_TOO_SHORT,
} TL_ERR;

} /* namespace TagLEET */

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#endif /* _TL_TYPES_H_ */
