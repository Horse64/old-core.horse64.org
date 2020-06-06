
#include <assert.h>

#include "compiler/ast.h"
#include "compiler/astparser.h"
#include "compiler/asttransform.h"
#include "compiler/compileproject.h"
#include "compiler/varstorage.h"


static h64expression *surroundingfunc(h64expression *expr) {
    while (expr->parent) {
        expr = expr->parent;
        if (expr->type == H64EXPRTYPE_FUNCDEF_STMT ||
                expr->type == H64EXPRTYPE_INLINEFUNCDEF)
            return expr;
    }
    return NULL;
}

static int movableassignvalue(h64expression *expr) {
    if (expr->type == H64EXPRTYPE_IDENTIFIERREF) {
        return 1;
    } else if (expr->type == H64EXPRTYPE_LITERAL) {
        return 1;
    }
    return 0;
}

static int movabledef(h64expression *expr) {
    if (expr->type == H64EXPRTYPE_VARDEF_STMT) {
        if (expr->vardef.value == NULL)
            return 1;
        if (movableassignvalue(expr->vardef.value))
            return 1;
    }
    return 0;
}

int _resolvercallback_AssignNonglobalStorage_visit_in(
        h64expression *expr, h64expression *parent, void *ud
        ) {
    asttransforminfo *rinfo = (asttransforminfo *)ud;

    if (expr->type == H64EXPRTYPE_FUNCDEF_STMT ||
            expr->type == H64EXPRTYPE_INLINEFUNCDEF) {
        if (!expr->funcdef._storageinfo) {
            expr->funcdef._storageinfo = malloc(
                sizeof(*expr->funcdef._storageinfo)
            );
            if (!expr->funcdef._storageinfo) {
                rinfo->hadoutofmemory = 1;
                return 0;
            }
            memset(expr->funcdef._storageinfo, 0,
                   sizeof(*expr->funcdef._storageinfo));
        }
        h64storageextrainfo *einfo = expr->funcdef._storageinfo;
        if (einfo->closureboundvars_count > 0) {
            // It's a closure! Assign temporaries for actual values
            // (the parameter temporaries are used for the varbox
            // reference, not the actual value)
            int param_used_temps = (
                einfo->closureboundvars_count +
                expr->funcdef.arguments.arg_count
            );
            int freetemp = param_used_temps;
            int i = 0;
            while (i < einfo->closureboundvars_count) {
                h64localstorageassign *newlstoreassign = realloc(
                    einfo->lstoreassign,
                    sizeof(*newlstoreassign) *
                    (einfo->lstoreassign_count + 1)
                );
                if (!newlstoreassign) {
                    rinfo->hadoutofmemory = 1;
                    return 0;
                }
                einfo->lstoreassign = newlstoreassign;
                memset(
                    &einfo->lstoreassign[einfo->lstoreassign_count], 0,
                    sizeof(*newlstoreassign)
                );
                einfo->lstoreassign[einfo->lstoreassign_count].
                    valuetemporaryid = freetemp;
                einfo->lstoreassign[einfo->lstoreassign_count].
                    valueboxtemporaryid = i;
                einfo->lstoreassign[einfo->lstoreassign_count].
                    vardef = einfo->closureboundvars[i];
                einfo->lstoreassign[einfo->lstoreassign_count].
                    use_start_token_index = (
                    einfo->closureboundvars[i]->first_use_token_index
                    );
                if (!movabledef(einfo->closureboundvars[i]->
                                declarationexpr)) {
                    if (einfo->closureboundvars[i]->declarationexpr->
                            tokenindex <
                            einfo->closureboundvars[i]->
                            first_use_token_index) {
                        einfo->lstoreassign[einfo->lstoreassign_count].
                            use_start_token_index
                            = (
                            einfo->closureboundvars[i]->declarationexpr->
                            tokenindex);
                    }
                }
                einfo->lstoreassign[einfo->lstoreassign_count].
                    use_end_token_index = (
                    einfo->closureboundvars[i]->last_use_token_index
                    );
                freetemp++;
                i++;
            }
            einfo->temps_for_locals_startindex = freetemp;
            einfo->lowest_guaranteed_free_temp = freetemp - 1;
        } else {
            einfo->temps_for_locals_startindex = (
                einfo->closureboundvars_count +
                expr->funcdef.arguments.arg_count
            );
        }
    }
    return 1;
}

int _resolvercallback_AssignNonglobalStorage_visit_out(
        h64expression *expr, h64expression *parent, void *ud
        ) {
    asttransforminfo *rinfo = (asttransforminfo *)ud;

    if (expr->type == H64EXPRTYPE_VARDEF_STMT ||
            expr->type == H64EXPRTYPE_FUNCDEF_STMT) {
        h64expression *func = surroundingfunc(expr);
        if (!func)
            return 1;  // global, we don't care about that
        if (!func->funcdef._storageinfo) {
            rinfo->hadunexpectederror = 1;
            return 0;
        }

        // Get scope, and figure out relevant usage range:
        h64scope *scope = (
            expr->type == H64EXPRTYPE_VARDEF_STMT ?
            expr->vardef.foundinscope : expr->funcdef.foundinscope
        );
        h64scopedef *scopedef = scope_QueryItem(
            scope,
            (expr->type == H64EXPRTYPE_VARDEF_STMT ?
             expr->vardef.identifier : expr->funcdef.name),
            0
        );
        assert(scopedef != NULL);
        int own_token_start = scopedef->first_use_token_index;
        int own_token_end = scopedef->first_use_token_index;
        if (!movabledef(expr) && expr->tokenindex < own_token_start) {
            own_token_start = expr->tokenindex;
        }

        // Determine temporary slot to be used:
        h64storageextrainfo *einfo = func->funcdef._storageinfo;
        int bestreusabletemp = -1;
        int bestreusebletemp_score = -1;
        int k = 0;
        while (k < einfo->lstoreassign_count) {
            int score = -1;
            if (einfo->lstoreassign[k].use_end_token_index <
                    own_token_start) {
                score = INT_MAX - (
                    own_token_start -
                    einfo->lstoreassign[k].use_end_token_index
                );
            } else if (einfo->lstoreassign[k].use_start_token_index >
                    own_token_end) {
                score = INT_MAX - (
                    own_token_end -
                    einfo->lstoreassign[k].use_start_token_index
                );
            }
            if (score > 0 && (
                    score < bestreusebletemp_score ||
                    bestreusabletemp < 0)) {
                bestreusabletemp = einfo->lstoreassign[k].valuetemporaryid;
                bestreusebletemp_score = score;
            }
            k++;
        }
        if (bestreusabletemp < 0) {
            bestreusabletemp = einfo->lowest_guaranteed_free_temp;
            einfo->lowest_guaranteed_free_temp++;
        }
        assert(bestreusabletemp >= 0);
        int valueboxid = -1;
        if (scopedef->closurebound) {
            valueboxid = einfo->lowest_guaranteed_free_temp;
            einfo->lowest_guaranteed_free_temp++;
        }

        // Insert actual temporary assignment:
        h64localstorageassign *newlstoreassign = realloc(
            einfo->lstoreassign,
            sizeof(*newlstoreassign) *
            (einfo->lstoreassign_count + 1)
        );
        if (!newlstoreassign) {
            rinfo->hadoutofmemory = 1;
            return 0;
        }
        einfo->lstoreassign = newlstoreassign;
        memset(
            &einfo->lstoreassign[einfo->lstoreassign_count], 0,
            sizeof(*newlstoreassign)
        );
        einfo->lstoreassign[einfo->lstoreassign_count].
            valuetemporaryid = bestreusabletemp;
        einfo->lstoreassign[einfo->lstoreassign_count].
            valueboxtemporaryid = valueboxid;
        einfo->lstoreassign[einfo->lstoreassign_count].
            vardef = scopedef;
        einfo->lstoreassign[einfo->lstoreassign_count].
            use_start_token_index = own_token_start;
        einfo->lstoreassign[einfo->lstoreassign_count].
            use_end_token_index = own_token_end;
    }
    return 1;
}

int varstorage_AssignLocalStorage(
        h64compileproject *pr, h64ast *ast
        ) {
    // Assign storage for all local variables and parameters:
    int transformresult = asttransform_Apply(
        pr, ast,
        &_resolvercallback_AssignNonglobalStorage_visit_in,
        &_resolvercallback_AssignNonglobalStorage_visit_out,
        NULL
    );
    if (!transformresult)
        return 0;

    return 1;
}

void varstorage_FreeExtraInfo(
        h64storageextrainfo *einfo
        ) {
    if (!einfo)
        return;
    free(einfo->lstoreassign);
    free(einfo->closureboundvars);
    free(einfo);
}
