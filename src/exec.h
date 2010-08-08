/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The interface to the ZINC interpreter.
*/

#ifndef EXEC_H_INCLUDED
#define EXEC_H_INCLUDED

extern int
exec_battle (battle_status_t *status, user_wish_t *cmd,
             unsigned int *end_warrior);

#endif /* EXEC_H_INCLUDED */
