/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The interface to the SDL-based user interface.
*/

#ifndef SDLUI_H_INCLUDED
#define SDLUI_H_INCLUDED

extern int sdlui_init (bool full_screen);

extern user_wish_t sdlui_start_battle (void);

extern user_wish_t sdlui_update_battle (unsigned int curr_warrior,
                                        cell_addr_t mod_cells,
                                        unsigned int execed_insns);

extern user_wish_t sdlui_finish_battle (battle_status_t status,
                                        unsigned int end_warrior);

extern int sdlui_quit (void);

#endif /* SDLUI_H_INCLUDED */
