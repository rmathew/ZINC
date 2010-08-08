/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  Expressions.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "zinc.h"
#include "zasm.h"
#include "expr.h"
#include "sym.h"


/* Allocate an expression node. Returns a pointer to such a node. */
expr_t *
alloc_expr (void)
{
  expr_t *ret_val = (expr_t *)malloc (sizeof (expr_t));
  ret_val->type = EXPR_NUMBER;
  ret_val->u.num_val = 0U;
  ret_val->u.identifier = NULL;
  ret_val->u.op[0] = NULL;
  ret_val->u.op[1] = NULL;
  return ret_val;
}


/* Frees an expression tree starting with the node EXPR. Note that EXPR
   can legally be NULL here. */
void
free_expr (expr_t *expr)
{
  if (expr != NULL)
  {
    switch (expr->type)
    {
    case EXPR_NEGATE:
    case EXPR_ADD:
    case EXPR_SUBTRACT:
    case EXPR_MULTIPLY:
    case EXPR_DIVIDE:
    case EXPR_MODULUS:
      /* Free operand expressions (Note that NULL is OK). */
      free_expr (expr->u.op[0]);
      free_expr (expr->u.op[1]);
      expr->u.op[0] = expr->u.op[1] = NULL;
      break;

    case EXPR_IDENTIFIER:
      expr->u.identifier = NULL;
      break;

    case EXPR_NUMBER:
      expr->u.num_val = 0U;
      break;

    default:
      input_error ("Internal Error (Invalid expression type in free_expr)",
                   expr->src_line);
      break;
    }
    
    /* Free this node. */
    free (expr);
  }
}


/* Evaluates the value of the expression EXPR found at the location FOR_PC
   in the core. ERR is set to 1 in case of an error. Returns the value of
   the expression, if successfully evaluated. */
int32_t
eval_expr (expr_t *expr, cell_addr_t for_pc, int *err)
{
  int32_t ret_val = 0;

  if (expr == NULL)
  {
    return 0;
  }

  int32_t op1, op2;
  sym_val_t *ident_val;
  switch (expr->type)
  {
  case EXPR_NUMBER:
    ret_val = (int32_t)(uint32_t )expr->u.num_val;
    break;

  case EXPR_IDENTIFIER:
    ident_val = get_sym (expr->u.identifier);
    if (ident_val != NULL && ident_val->type != SYM_UNDEFINED)
    {
      if (ident_val->type == SYM_LABEL)
      {
        ret_val
          = (int32_t)(uint32_t)(ident_val->u.const_val + core_size - for_pc);
      }
      else if (ident_val->type == SYM_CONSTANT)
      {
        ret_val = (int32_t)ident_val->u.const_val;
      }
      else
      {
        /* It is a SYM_EXPR. Note that we pass 0 to eval_expr() for FOR_PC,
           so that identifiers defined using DEF have the same value
           everywhere. */
        ret_val = eval_expr (ident_val->u.expr, 0U, err);
      }
    }
    else
    {
      char tmp_buf[TMP_BUF_SIZE];
      snprintf (tmp_buf, TMP_BUF_SIZE - 1, "Undefined symbol \"%s\"",
                expr->u.identifier);
      tmp_buf[TMP_BUF_SIZE - 1] = '\0';

      input_error (tmp_buf, expr->src_line);

      *err = 1;
    }
    break;

  case EXPR_ADD:
    op1 = eval_expr (expr->u.op[0], for_pc, err);
    op2 = eval_expr (expr->u.op[1], for_pc, err);
    ret_val = op1 + op2;
    break;

  case EXPR_SUBTRACT:
    op1 = eval_expr (expr->u.op[0], for_pc, err);
    op2 = eval_expr (expr->u.op[1], for_pc, err);
    ret_val = op1 - op2;
    break;

  case EXPR_MULTIPLY:
    op1 = eval_expr (expr->u.op[0], for_pc, err);
    op2 = eval_expr (expr->u.op[1], for_pc, err);
    ret_val = op1 * op2;
    break;

  case EXPR_DIVIDE:
    op1 = eval_expr (expr->u.op[0], for_pc, err);
    op2 = eval_expr (expr->u.op[1], for_pc, err);
    if (op2 != 0U)
    {
      ret_val = op1 / op2;
    }
    else
    {
      input_error ("Division by zero", expr->u.op[1]->src_line);

      *err = 1;
    }
    break;

  case EXPR_MODULUS:
    op1 = eval_expr (expr->u.op[0], for_pc, err);
    op2 = eval_expr (expr->u.op[1], for_pc, err);
    if (op2 != 0U)
    {
      ret_val = op1 % op2;
    }
    else
    {
      input_error ("Division by zero", expr->u.op[1]->src_line);

      *err = 1;
    }
    break;

  case EXPR_NEGATE:
    op1 = eval_expr (expr->u.op[0], for_pc, err);
    ret_val = -1 * op1;
    break;

  default:
    input_error ("Internal Error (Invalid expression type in eval_expr)", 
                 expr->src_line);
    *err = 1;
    break;
  }

  return ret_val;
}
