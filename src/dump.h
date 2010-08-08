/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  Methods to dump instructions and programmes.
*/

#ifndef DUMP_H_INCLUDED
#define DUMP_H_INCLUDED

extern void dump_insn (char *buf, size_t buf_size, cell_t *c);
extern void dump_warrior (warrior_t *w);

#endif /* DUMP_H_INCLUDED */
