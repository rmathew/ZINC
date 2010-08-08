/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  Expressions.
*/

#ifndef EXPR_H_INCLUDED
#define EXPR_H_INCLUDED

/* The types of expression. */
typedef enum
{
  EXPR_NUMBER,
  EXPR_IDENTIFIER,
  EXPR_ADD,
  EXPR_SUBTRACT,
  EXPR_MULTIPLY,
  EXPR_DIVIDE,
  EXPR_MODULUS,
  EXPR_NEGATE,
} expr_type_t;

/* A node in an expression tree. */
typedef struct expr
{
  expr_type_t type;

  union
  {
    cell_addr_t num_val;
    const char *identifier;
    struct expr *op[2];
  } u;

  line_t src_line;
} expr_t;

extern expr_t *alloc_expr (void);

extern void free_expr (expr_t *expr);

extern int32_t eval_expr (expr_t *expr, cell_addr_t for_pc, int *err);

#endif /* EXPR_H_INCLUDED */
