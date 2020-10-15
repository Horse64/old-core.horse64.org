// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HORSE64_COMPILER_LEXER_H_
#define HORSE64_COMPILER_LEXER_H_

#include "compileconfig.h"

#include <stdint.h>
#include <stdlib.h>  // for NULL

#include "compiler/result.h"
#include "compiler/warningconfig.h"
#include "json.h"

typedef enum h64tokentype {
    H64TK_INVALID = 0,
    H64TK_IDENTIFIER = 1,
    H64TK_BRACKET,
    H64TK_COMMA,
    H64TK_COLON,
    H64TK_KEYWORD,
    H64TK_CONSTANT_INT,
    H64TK_CONSTANT_FLOAT,
    H64TK_CONSTANT_BOOL,
    H64TK_CONSTANT_NONE,
    H64TK_CONSTANT_STRING,
    H64TK_CONSTANT_BYTES,
    H64TK_BINOPSYMBOL,
    H64TK_UNOPSYMBOL,
    H64TK_INLINEFUNC,
    H64TK_MAPARROW
} h64tokentype;

typedef struct h64token {
    h64tokentype type;
    union {
        double float_value;
        int64_t int_value;
        char *str_value;
        uint8_t char_value;
    };
    int str_value_len;
    int64_t line, column;
} h64token;

typedef struct h64tokenizedfile {
    h64result resultmsg;
    int token_count;
    h64token *token;
} h64tokenizedfile;

ATTR_UNUSED static char *h64keywords[] = {
    "async", "noasync", "async", "const",
    "if", "while", "func",
    "for", "from", "with",
    "var", "class", "extends",
    "import", "else", "elseif",
    "break", "continue", "do",
    "rescue", "finally", "error",
    "new", "return", "in", "as",
    "getter", "setter", "value",
    "deprecated", "multiarg",
    NULL
};

h64tokenizedfile lexer_ParseFromFile(
    const char *fileuri, h64compilewarnconfig *wconfig,
    int vfsflags
);

void lexer_ClearToken(h64token *t);

void lexer_FreeFileTokens(h64tokenizedfile *tfile);

const char *lexer_TokenTypeToStr(h64tokentype type);

char *lexer_TokenToJSONStr(h64token *t, const char *fileuri);

jsonvalue *lexer_TokenToJSON(h64token *t, const char *fileuri);

int is_valid_utf8_char(
    const unsigned char *p, int size
);

int utf8_char_len(const unsigned char *p);

void lexer_DebugPrintTokens(h64token *t, int count);

#endif  // HORSE64_COMPILER_LEXER_H_
