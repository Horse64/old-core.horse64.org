
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "compiler/ast.h"
#include "compiler/asthelpers.h"
#include "compiler/astparser.h"
#include "compiler/asttransform.h"
#include "compiler/codegen.h"
#include "compiler/compileproject.h"
#include "compiler/varstorage.h"
#include "unicode.h"


int newcalctemp(h64expression *func) {
    assert(func->funcdef._storageinfo != NULL);
    func->funcdef._storageinfo->_temp_calc_slots_used_right_now++;
    if (func->funcdef._storageinfo->_temp_calc_slots_used_right_now >
            func->funcdef._storageinfo->temp_calculation_slots)
        func->funcdef._storageinfo->temp_calculation_slots = (
            func->funcdef._storageinfo->_temp_calc_slots_used_right_now
        );
    return func->funcdef._storageinfo->_temp_calc_slots_used_right_now;
}

int appendinst(
        h64program *p,
        h64expression *func, void *ptr, size_t len
        ) {
    assert(p != NULL);
    assert(func != NULL && (func->type == H64EXPRTYPE_FUNCDEF_STMT ||
           func->type == H64EXPRTYPE_INLINEFUNCDEF));
    int id = func->funcdef.bytecode_func_id;
    assert(id >= 0 && id < p->func_count);
    char *instructionsnew = realloc(
        p->func[id].instructions,
        sizeof(*p->func[id].instructions) *
        (p->func[id].instructions_bytes + len)
    );
    if (!instructionsnew) {
        return 0;
    }
    p->func[id].instructions = instructionsnew;
    memcpy(
        p->func[id].instructions + p->func[id].instructions_bytes,
        ptr, len
    );
    p->func[id].instructions_bytes += len;
    return 1;
}

int _codegencallback_DoCodegen_visit_out(
        h64expression *expr, h64expression *parent, void *ud
        ) {
    asttransforminfo *rinfo = (asttransforminfo *)ud;

    h64expression *func = surroundingfunc(expr);
    if (!func) {
        return 1;
    }

    if (expr->type == H64EXPRTYPE_LITERAL) {
        int temp = newcalctemp(func);
        h64instruction_setconst inst = {0};
        inst.type = H64INST_SETCONST;
        memset(&inst.content, 0, sizeof(inst.content));
        if (expr->literal.type == H64TK_CONSTANT_INT) {
            inst.content.type = H64VALTYPE_INT64;
            inst.content.int_value = expr->literal.int_value;
        } else if (expr->literal.type == H64TK_CONSTANT_FLOAT) {
            inst.content.type = H64VALTYPE_FLOAT64;
            inst.content.float_value = expr->literal.float_value;
        } else if (expr->literal.type == H64TK_CONSTANT_BOOL) {
            inst.content.type = H64VALTYPE_BOOL;
            inst.content.int_value = expr->literal.int_value;
        } else if (expr->literal.type == H64TK_CONSTANT_NONE) {
            inst.content.type = H64VALTYPE_NONE;
        } else if (expr->literal.type == H64TK_CONSTANT_STRING) {
            inst.content.type = H64VALTYPE_SHORTSTR;
            assert(expr->literal.str_value != NULL);
            int64_t out_len = 0;
            int abortinvalid = 0;
            int abortoom = 0;
            unicodechar *result = utf8_to_utf32_ex(
                expr->literal.str_value,
                strlen(expr->literal.str_value),
                NULL, NULL, &out_len, 1,
                &abortinvalid, &abortoom
            );
            if (!result) {
                if (abortoom) {
                    rinfo->hadoutofmemory = 1;
                    return 0;
                }
                char buf[256];
                snprintf(buf, sizeof(buf) - 1,
                    "internal error: utf8 to utf32 "
                    "conversion unexpectedly failed"
                );
                if (!result_AddMessage(
                        &rinfo->ast->resultmsg,
                        H64MSG_ERROR, buf,
                        rinfo->ast->fileuri,
                        expr->line, expr->column
                        )) {
                    rinfo->hadoutofmemory = 1;
                    return 0;
                }
                return 1;
            }
            assert(!abortinvalid);
            assert(!abortoom);
            if (out_len <= VALUECONTENT_SHORTSTRLEN) {
                memcpy(
                    inst.content.shortstr_value,
                    result, out_len * sizeof(*result)
                );
                inst.content.type = H64VALTYPE_SHORTSTR;
            } else {
                inst.content.type = H64VALTYPE_CONSTPREALLOCSTR;
                inst.content.constpreallocstr_value = malloc(
                    out_len * sizeof(*result)
                );
                if (!inst.content.constpreallocstr_value) {
                    rinfo->hadoutofmemory = 1;
                    return 0;
                }
                inst.content.constpreallocstr_len = out_len;
                memcpy(
                    inst.content.constpreallocstr_value,
                    result, out_len * sizeof(*result)
                );
            }
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf) - 1,
                "internal error: unhandled literal type %d",
                (int)expr->literal.type
            );
            if (!result_AddMessage(
                    &rinfo->ast->resultmsg,
                    H64MSG_ERROR, buf,
                    rinfo->ast->fileuri,
                    expr->line, expr->column
                    )) {
                rinfo->hadoutofmemory = 1;
                return 0;
            }
            return 1;
        }
        if (!appendinst(rinfo->pr->program, func,
                        &inst, sizeof(inst))) {
            rinfo->hadoutofmemory = 1;
            return 0;
        }
        expr->storage._exprstoredintemp = temp;
    } else if (expr->type == H64EXPRTYPE_FUNCDEF_STMT ||
            expr->type == H64EXPRTYPE_CLASSDEF_STMT) {
        // Nothing to do
    } else if ((expr->type == H64EXPRTYPE_VARDEF_STMT &&
                expr->vardef.value != NULL) ||
            (expr->type == H64EXPRTYPE_ASSIGN_STMT && (
             expr->assignstmt.lvalue->type ==
                H64EXPRTYPE_IDENTIFIERREF ||
             (expr->assignstmt.lvalue->type ==
                  H64EXPRTYPE_BINARYOP &&
              expr->assignstmt.lvalue->op.optype ==
                  H64OP_MEMBERBYIDENTIFIER &&
              expr->assignstmt.lvalue->op.value2->type ==
                  H64EXPRTYPE_IDENTIFIERREF &&
              expr->assignstmt.lvalue->op.value2->storage.set)))) {
        // Assigning directly to a variable (rather than a member,
        // map value, or the like)
        int assignfromtemporary = -1;
        storageref *str = NULL;
        if (expr->type == H64EXPRTYPE_VARDEF_STMT) {
            assert(expr->storage.set);
            str = &expr->storage.ref;
            assignfromtemporary = expr->vardef.value->
                storage._exprstoredintemp;
        } else if (expr->type == H64EXPRTYPE_ASSIGN_STMT) {
            if (expr->assignstmt.lvalue->type ==
                    H64EXPRTYPE_BINARYOP) {
                str = &expr->assignstmt.lvalue->op.value2->storage.ref;
            } else {
                assert(expr->assignstmt.lvalue->storage.set);
                str = &expr->assignstmt.lvalue->storage.ref;
            }
            assignfromtemporary = (
                expr->assignstmt.rvalue->storage._exprstoredintemp
            );
        }
        assert(assignfromtemporary >= 0);
        assert(str->type == H64STORETYPE_GLOBALVARSLOT ||
               str->type == H64STORETYPE_STACKSLOT);
        if (str->type == H64STORETYPE_GLOBALVARSLOT) {
            h64instruction_setglobal inst = {0};
            inst.type = H64INST_SETGLOBAL;
            inst.globalto = str->id;
            inst.slotfrom = assignfromtemporary;
            if (!appendinst(rinfo->pr->program, func,
                            &inst, sizeof(inst))) {
                rinfo->hadoutofmemory = 1;
                return 0;
            }
        } else {
            assert(str->type == H64STORETYPE_STACKSLOT);
            h64instruction_valuecopy inst = {0};
            inst.type = H64INST_VALUECOPY;
            inst.slotto = str->id;
            inst.slotfrom = assignfromtemporary;
            if (!appendinst(rinfo->pr->program, func,
                            &inst, sizeof(inst))) {
                rinfo->hadoutofmemory = 1;
                return 0;
            }
        }
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf) - 1,
            "internal error: unhandled literal type %d",
            (int)expr->literal.type
        );
        if (!result_AddMessage(
                &rinfo->ast->resultmsg,
                H64MSG_ERROR, buf,
                rinfo->ast->fileuri,
                expr->line, expr->column
                )) {
            rinfo->hadoutofmemory = 1;
            return 0;
        }
        return 1;
    }

    if (IS_STMT(expr->type)) {
        func->funcdef._storageinfo->_temp_calc_slots_used_right_now = 0;
    }

    return 1;
}

int codegen_GenerateBytecodeForFile(
        h64compileproject *project, h64ast *resolved_ast
        ) {
    if (!project || !resolved_ast)
        return 0;

    // Do actual codegen step:
    int transformresult = asttransform_Apply(
        project, resolved_ast,
        NULL, &_codegencallback_DoCodegen_visit_out,
        NULL
    );
    if (!transformresult)
        return 0;

    return 1;
}