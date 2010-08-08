/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The interface to the ZINC assembler.
*/

#ifndef ZASM_H_INCLUDED
#define ZASM_H_INCLUDED

/* Represents a line number. */
typedef unsigned int line_t;

/* Represents a column number. */
typedef unsigned int col_t;

extern int assemble_warrior (warrior_t *warrior);

extern void input_error (const char *msg, line_t where);

#endif /* ZASM_H_INCLUDED */
