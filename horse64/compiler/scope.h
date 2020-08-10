// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#ifndef HORSE64_SCOPE_H_
#define HORSE64_SCOPE_H_


typedef struct h64expression h64expression;
typedef struct h64scope h64scope;


typedef struct jsonvalue jsonvalue;

typedef struct hashmap hashmap;

typedef struct h64scopedef h64scopedef;

typedef struct h64scopedef {
    int classandfuncnestinglevel;

    h64expression *declarationexpr;
    int additionaldecl_count;
    h64expression **additionaldecl;

    const char *identifier;
    int everused, closurebound, expanded_to_real_use_range;
    h64scope *scope;
} h64scopedef;

#define SCOPEMAGICINITNUM (0xAAFF00AA)

typedef struct h64scope {
    int definitionref_count, definitionref_alloc;
    h64scopedef **definitionref;

    unsigned int magicinitnum;

    int classandfuncnestinglevel;
    h64scope *parentscope;
    int is_global;

    hashmap *name_to_declaration_map;
    h64expression *expr;
} h64scope;


int scope_Init(h64scope *scope, h64expression *expr);

int scope_AddItem(
    h64scope *scope, const char *identifier_ref,
    h64expression *expr, int *outofmemory
);

void scope_RemoveItem(
    h64scope *scope, const char *identifier_ref
);

#define SCOPEQUERY_FLAG_BUBBLEUP 1
#define SCOPEQUERY_FLAG_QUERYCLASSITEMS 2

h64scopedef *scope_QueryItem(
    h64scope *scope, const char *identifier_ref, int flags
);

void scope_FreeData(h64scope *scope);

char *scope_ScopeToJSONStr(h64scope *scope);

jsonvalue *scope_ScopeToJSON(
    h64scope *scope
);

#endif  // HORSE64_SCOPE_H_
