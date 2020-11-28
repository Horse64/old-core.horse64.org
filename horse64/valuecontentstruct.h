// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HORSE64_VALUECONTENTSTRUCT_H_
#define HORSE64_VALUECONTENTSTRUCT_H_

#include <stdint.h>

#include "compiler/globallimits.h"
#include "vmschedule.h"
#include "widechar.h"


typedef enum valuetype {
    H64VALTYPE_NONE = 0,
    H64VALTYPE_INT64 = 1,
    H64VALTYPE_FLOAT64,
    H64VALTYPE_BOOL,
    H64VALTYPE_FUNCREF,
    H64VALTYPE_CLASSREF,
    H64VALTYPE_ERROR,
    H64VALTYPE_GCVAL,
    H64VALTYPE_SHORTSTR,
    H64VALTYPE_CONSTPREALLOCSTR,
    H64VALTYPE_SHORTBYTES,
    H64VALTYPE_CONSTPREALLOCBYTES,
    H64VALTYPE_VECTOR,
    H64VALTYPE_UNSPECIFIED_KWARG,
    H64VALTYPE_SUSPENDINFO,
    H64VALTYPE_TOTAL
} valuetype;

#define VALUECONTENT_SHORTSTRLEN 2
#define VALUECONTENT_SHORTBYTESLEN 7

typedef struct h64errorinfo h64errorinfo;
typedef struct valuecontent valuecontent;
typedef struct vectorentry vectorentry;


typedef struct valuecontent {
    uint8_t type;
    union {
        int64_t int_value;  // 8 bytes
        double float_value;   // 8 bytes
        void *ptr_value;   // 4 or 8 bytes
        struct {   // 9 bytes
            uint8_t shortstr_len;
            h64wchar shortstr_value[
                VALUECONTENT_SHORTSTRLEN
            ];  // should be 2byte/16bit aligned
        } __attribute__((packed));
        struct {   // 8 bytes
            uint8_t shortbytes_len;
            char shortbytes_value[
                VALUECONTENT_SHORTBYTESLEN
            ];  // should be 2byte/16bit aligned
        } __attribute__((packed));
        struct {   // 12 bytes
            h64wchar *constpreallocstr_value;
            int32_t constpreallocstr_len;
        } __attribute__((packed));
        struct {   // 12 bytes
            h64wchar *constpreallocbytes_value;
            int32_t constpreallocbytes_len;
        } __attribute__((packed));
        struct {   // 12 bytes
            classid_t error_class_id;
            h64errorinfo *einfo;
        } __attribute__((packed));
        struct {  // 12 bytes
            int32_t vector_len;
            vectorentry *vector_values;
        } __attribute__((packed));
        struct {  // 12 bytes
            int suspend_type;
            int64_t suspend_intarg;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed)) valuecontent;

#endif  // HORSE64_VALUECONTENTSTRUCT_H_