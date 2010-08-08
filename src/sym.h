/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The interface to the symbol table of the ZINC assembler.
*/

#ifndef SYM_H_INCLUDED
#define SYM_H_INCLUDED

/* The type of a symbol in the symbol table. */
typedef enum
{
  SYM_CONSTANT,
  SYM_LABEL,
  SYM_EXPR,
  SYM_UNDEFINED,
} sym_type_t;

/* The value of a symbol stored in the symbol table. */
typedef struct sym_val
{
  /* The name of the symbol. */
  char *name;

  /* The type of the symbol. */
  sym_type_t type;

  /* The actual value of the symbol (a constant or an expression). */
  union
  {
    cell_addr_t const_val;
    expr_t *expr;
  } u;
} sym_val_t;


extern sym_val_t *get_sym (const char *s);

extern void put_sym (const char *name, sym_val_t *value);

extern void clear_syms (void);

#endif /* SYM_H_INCLUDED */
