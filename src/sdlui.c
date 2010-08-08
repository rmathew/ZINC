/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The SDL-based user interface. (FIXME: Ugly code.)
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "zinc.h"
#include "dump.h"
#include "sdlui.h"
#include "sdltxt.h"

/* The surface representing the user interface. */
static SDL_Surface *screen = NULL;

/* A flag that indicates if we are paused or not. */
static bool paused;

/* A flag to indicate if the user is inspecting core or not. */
static bool inspecting;

/* The cell where the PC of the current task was before the user went
   wandering around in the core inspecting cells. */
static cell_addr_t inspect_orig_pc;

/* The maximum number of updates to the core display that should be queued
   before actually showing them on screen. This delayed update gives a huge
   performance boost - the bigger the size of the queue, the better - to
   the rendering of the core. */
#define MAX_QUEUED_UPDATES 100U

/* The actual number of queued display updates. */
static unsigned int queued_updates = 0U;

/* The minimum number of milliseconds that a batch of queued updates should
   take to render. If the actual number falls short, insert an appropriate
   delay. This kludge allows us to run at the same effective speed on PCs
   with different processor speeds. */
static Uint32 min_ms_per_batch = 0U;

/* The address of the last cell around which a PC-indicator was drawn. */
static cell_addr_t prev_drawn_pc;

static SDL_Color bg_clr;
static SDL_Color default_clr;
static SDL_Color border_clr;
static SDL_Color warrior_clrs[3];

static Uint32 bg_clr_num;
static Uint32 default_clr_num;
static Uint32 border_clr_num;
static Uint32 warrior_clr_nums[3];

static Uint16 font_width;
static Uint16 font_height;

/* The width, in pixels, of the screen. */
static Uint16 scr_width = 640;

/* The height, in pixels, of the screen. */
static Uint16 scr_height = 480;

/* The bytes (not bits) per pixel of the screen surface. */
static Uint8 scr_bypp;

/* The number of cells to render per row of the displayed core.
   FIXME: Hard-coded value. */
static Uint16 num_x_cells = 125;

/* The number of cells to render per column of the displayed core. Must be
   equal to (CORE_SIZE / num_x_cells). FIXME: Hard-coded value. */
static Uint16 num_y_cells = 64;

/* A cell is rendered by a square box this number of pixels wide. */
static Uint16 cell_size = 3;

/* The number of pixels used for general spacing among rendered stuff. */
static Uint16 gutter_size = 3;

/* The position and dimensions of the area that displays the core. */
static SDL_Rect core_rect;

/* The position and dimensions of the area that displays the timer that
   shows how much time is left before a battle is declared a tie. */
static SDL_Rect timer_rect;

/* The position and dimensions of the area that displays the status. */
static SDL_Rect stat_rect;

/* The maximum number of characters to allow to be displayed in the status
   area. */
static Uint16 max_stat_chars;

/* Message to show in the status area when the battle is paused. */
static char *paused_stat_msg
  = "Paused (SPACE = Continue, ENTER = Step, R = Reload, ESC = Quit)";

/* Message to show in the status area when the battle is running. */
static char *running_stat_msg
  = "Running (SPACE = Pause, R = Reload, ESC = Quit)";

/* Message to show in the status area when the user is inspecting core. */
static char *inspecting_stat_msg
  = "Inspecting (UP/DOWN/RIGHT/LEFT = Navigate, ENTER = Set, ESC = Reset)";


/* Draws a cell with the given address C in the core. */
static void
draw_cell (cell_addr_t c)
{
  warrior_id_t marker = core[c].marker;

  SDL_Rect rect;
  rect.x = core_rect.x + (cell_size + 2) * (c % num_x_cells) + 1;
  rect.y = core_rect.y + (cell_size + 2) * (c / num_x_cells) + 1;
  rect.w = rect.h = cell_size;

  Uint32 clr = warrior_clr_nums[marker];
  SDL_FillRect (screen, &rect, clr);
}


/* Writes the message MSG into the status area. */
static void
write_status (const char *msg)
{
  int len = strlen (msg);
  Uint16 x = stat_rect.x;

  if (len < max_stat_chars)
  {
    x = font_width * (max_stat_chars - len) / 2;
  }

  SDL_FillRect (screen, &stat_rect, bg_clr_num);
  sdltxt_write (msg, max_stat_chars, screen, x, stat_rect.y);
}


/* Draws a horizontal line as fast as possible in the current display depth.
   The line is drawn from (X,Y), is W units wide and is drawn in the colour
   CLR_NUM. */
static void
draw_x_line (Uint16 x, Uint16 y, Uint16 w, Uint32 clr_num)
{
  Uint8 *p1, *p3;
  Uint16 *p2;
  Uint32 *p4;
  Uint8 r, g, b;
  Uint8 rb, gb, bb;
  
  switch (scr_bypp)
  {
  case 1:
    p1 = (Uint8 *)screen->pixels + y * screen->pitch + x;
    memset (p1, clr_num, w);
    break;

  case 2:
    p2 = (Uint16 *)screen->pixels + y * screen->pitch / 2 + x;
    for (int i = 0; i < w; i++)
    {
      *p2 = clr_num;
      p2++;
    }
    break;

  case 3:
    SDL_GetRGB (clr_num, screen->format, &r, &g, &b);
    rb = screen->format->Rshift / 8;
    gb = screen->format->Gshift / 8;
    bb = screen->format->Bshift / 8;
    p3 = (Uint8 *)screen->pixels + y * screen->pitch + x;
    for (int i = 0; i < w; i++)
    {
      *(p3 + rb) = r;
      *(p3 + gb) = g;
      *(p3 + bb) = b;
      p3 += 3;
    }
    break;

  case 4:
    p4 = (Uint32 *)screen->pixels + y * screen->pitch / 4 + x;
    for (int i = 0; i < w; i++)
    {
      *p4 = clr_num;
      p4++;
    }
    break;
  }
}


/* Draws a vertical line from (X,Y), H units high and in the colour
   CLR_NUM.  */
static void
draw_y_line (Uint16 x, Uint16 y, Uint16 h, Uint32 clr_num)
{
  Uint8 *p1, *p3;
  Uint16 *p2;
  Uint32 *p4;
  Uint8 r, g, b;
  
  switch (scr_bypp)
  {
  case 1:
    p1 = (Uint8 *)screen->pixels + y * screen->pitch + x;
    for (int i = 0; i < h; i++)
    {
      *p1 = clr_num;
      p1 += screen->pitch;
    }
    break;

  case 2:
    p2 = (Uint16 *)screen->pixels + y * screen->pitch / 2 + x;
    for (int i = 0; i < h; i++)
    {
      *p2 = clr_num;
      p2 += screen->pitch / 2;
    }
    break;

  case 3:
    SDL_GetRGB (clr_num, screen->format, &r, &g, &b);
    p3 = (Uint8 *)screen->pixels + y * screen->pitch + x;
    for (int i = 0; i < h; i++)
    {
      *(p3 + screen->format->Rshift / 8) = r;
      *(p3 + screen->format->Gshift / 8) = g;
      *(p3 + screen->format->Bshift / 8) = b;
      p2 += screen->pitch;
    }
    break;

  case 4:
    p4 = (Uint32 *)screen->pixels + y * screen->pitch / 4 + x;
    for (int i = 0; i < h; i++)
    {
      *p4 = clr_num;
      p4 += screen->pitch / 4;
    }
    break;
  }
}


/* Draws a box at (X,Y) that is W units wide and H units high and is drawn
   in the colour CLR_NUM. */
static void
draw_box (Uint16 x, Uint16 y, Uint16 w, Uint16 h, Uint32 clr_num)
{
  draw_x_line (x, y, w, clr_num);
  draw_x_line (x, y + h - 1, w, clr_num);

  draw_y_line (x, y, h, clr_num);
  draw_y_line (x + w - 1, y, h, clr_num);
}


/* Highlights the PC of the warrior indicated by CURR_WARRIOR. The indicator
   at PREV_DRAWN_PC, if any, is erased. */
static void
draw_pc_ind (unsigned int curr_warrior)
{
  if (prev_drawn_pc != INVALID_CELL_ADDR)
  {
    Uint16 w = cell_size + 2;
    Uint16 h = cell_size + 2;
    Uint16 x
      = core_rect.x + (cell_size + 2) * (prev_drawn_pc % num_x_cells);
    Uint16 y
      = core_rect.y + (cell_size + 2) * (prev_drawn_pc / num_x_cells);

    draw_box (x, y, w, h, bg_clr_num);
    x -= 1U;
    y -= 1U;
    w += 2U;
    h += 2U;
    draw_box (x, y, w, h, bg_clr_num);
  }

  if (warriors[curr_warrior].tasks != NULL)
  {
    cell_addr_t curr_pc = warriors[curr_warrior].tasks->pc;

    Uint16 w = cell_size + 2;
    Uint16 h = cell_size + 2;
    Uint16 x = core_rect.x + (cell_size + 2) * (curr_pc % num_x_cells);
    Uint16 y = core_rect.y + (cell_size + 2) * (curr_pc / num_x_cells);

    draw_box (x, y, w, h, border_clr_num);
    x -= 1U;
    y -= 1U;
    w += 2U;
    h += 2U;
    draw_box (x, y, w, h, border_clr_num);

    prev_drawn_pc = curr_pc;
  }
}


/* Displays the instruction about to be executed for the warrior indicated
   by CURR_WARRIOR. */
static void
display_insn (unsigned int curr_warrior)
{
  /* Clear the area that shows the instruction about to be executed. */
  SDL_Rect rect;
  rect.x = gutter_size + curr_warrior * (scr_width / 2 - 2 * gutter_size);
  rect.y = core_rect.y - (6 * gutter_size + font_height);
  rect.w = scr_width / 2 - 2 * gutter_size;
  rect.h = font_height;
  SDL_FillRect (screen, &rect, bg_clr_num);

  /* For the warrior, show the instruction that is about to be executed. */
  if (warriors[curr_warrior].tasks != NULL)
  {
    cell_addr_t curr_pc = warriors[curr_warrior].tasks->pc;

    int max_chars = (scr_width / 2 - 2 * gutter_size) / font_width;

    Uint16 x = gutter_size + curr_warrior * (scr_width / 2 - 2 * gutter_size);
    Uint16 y = core_rect.y - (6 * gutter_size + font_height);

    char tmp_buf[TMP_BUF_SIZE];
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "@%04u:   ", curr_pc);
    dump_insn (tmp_buf + 9, TMP_BUF_SIZE - 10, &core[curr_pc]);
    tmp_buf[TMP_BUF_SIZE - 1] = '\0';

    sdltxt_write (tmp_buf, max_chars, screen, x, y);
  }
}


/* Displays the scores for the warriors at the end of a battle. */
static void
display_scores (void)
{
  int max_chars = (scr_width / 2 - 2 * gutter_size) / font_width;

  char tmp_buf[TMP_BUF_SIZE];
  for (int i = 0; i < num_warriors; i++)
  {
    Uint16 x = gutter_size + i * (scr_width / 2 - 2 * gutter_size);
    Uint16 y = core_rect.y - 7 * gutter_size - 2 * font_height;

    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "Score:   %u", warriors[i].score);
    tmp_buf[TMP_BUF_SIZE - 1] = '\0';
    sdltxt_write (tmp_buf, max_chars, screen, x, y);
  }
}


/* Draws the timer that shows how much time is left now that EXECED_INSNS
   cycles out of MAX_CYCLES have executed. */
static void
draw_timer (unsigned int execed_insns)
{
  Uint16 total_w = core_rect.w + 2;
  timer_rect.w
    = (execed_insns * (uint32_t )total_w) / max_cycles;
  timer_rect.x = core_rect.x - 1 + core_rect.w + 2 - timer_rect.w;
  SDL_FillRect (screen, &timer_rect, bg_clr_num);
}


/* Processes the user input event EVENT, given that the current warrior is
   CURR_WARRIOR, and indicates the user request in CMD. Returns if the
   simulation should proceed or not. */
static bool
sdlui_process_event (SDL_Event *event, user_wish_t *cmd,
                     unsigned int curr_warrior)
{
  bool proceed = false;

  switch (event->type)
  {
  case SDL_QUIT:
    *cmd = QUIT_ZINC;
    proceed = true;
    break;

  case SDL_KEYDOWN:
    switch (event->key.keysym.sym)
    {
    case SDLK_SPACE:
      if (paused == true)
      {
        write_status (running_stat_msg);
      }
      else
      {
        write_status (paused_stat_msg);
      }

      paused = !paused;
      proceed = !paused;
      break;

    case SDLK_RETURN:
      if (inspecting == true)
      {
        inspecting = false;
        write_status (paused_stat_msg);
        SDL_UpdateRect (screen, 0, 0, scr_width, scr_height);
        proceed = false;
      }
      else
      {
        proceed = true;
      }
      break;

    case SDLK_ESCAPE:
      if (inspecting == true)
      {
        inspecting = false;
        write_status (paused_stat_msg);
        if (warriors[curr_warrior].tasks != NULL)
        {
          warriors[curr_warrior].tasks->pc = inspect_orig_pc;
        }
        display_insn (curr_warrior);
        draw_pc_ind (curr_warrior);
        SDL_UpdateRect (screen, 0, 0, scr_width, scr_height);
        proceed = false;
      }
      else
      {
        *cmd = QUIT_ZINC;
        proceed = true;
      }
      break;

    case SDLK_r:
      *cmd = RELOAD_WARRIORS;
      proceed = true;
      break;

    case SDLK_UP:
    case SDLK_DOWN:
    case SDLK_RIGHT:
    case SDLK_LEFT:
      if (paused == true && warriors[curr_warrior].tasks != NULL)
      {
        inspecting = true;
        write_status (inspecting_stat_msg);

        cell_addr_t curr_pc = warriors[curr_warrior].tasks->pc;

        if (event->key.keysym.sym == SDLK_UP)
        {
          curr_pc = (curr_pc + core_size - num_x_cells) % core_size;
        }
        else if (event->key.keysym.sym == SDLK_DOWN)
        {
          curr_pc = (curr_pc + num_x_cells) % core_size;
        }
        else if (event->key.keysym.sym == SDLK_RIGHT)
        {
          curr_pc = (curr_pc + 1) % core_size;
        }
        else if (event->key.keysym.sym == SDLK_LEFT)
        {
          curr_pc = (curr_pc + core_size - 1) % core_size;
        }
        
        warriors[curr_warrior].tasks->pc = curr_pc;
        display_insn (curr_warrior);
        draw_pc_ind (curr_warrior);
        SDL_UpdateRect (screen, 0, 0, scr_width, scr_height);
      }
      proceed = false;
      break;

    default:
      proceed = false;
      break;
    }
    break;

  default:
    proceed = false;
    break;
  }

  return proceed;
}


/* Called by the simulator to indicate that the battle status should be
   updated. CURR_WARRIOR is the current warrior, MOD_CELL is the cell, if
   any, that has been modified and EXECED_INSNS is the total number of
   cycles lapsed in the current battle. Returns the wish of the user
   (proceed, quit, etc.) based on an input event, if any. */
user_wish_t
sdlui_update_battle (unsigned int curr_warrior, cell_addr_t mod_cell,
                     unsigned int execed_insns)
{
  user_wish_t cmd = CONTINUE_BATTLE;

  if (SDL_MUSTLOCK (screen))
  {
    if (SDL_LockSurface (screen) < 0)
    {
      return cmd;
    }
  }

  /* Draw only the cell modified in the last instruction, if any. */
  if (mod_cell != INVALID_CELL_ADDR)
  {
    draw_cell (mod_cell);
  }

  if (paused == true || queued_updates >= MAX_QUEUED_UPDATES)
  {
    for (unsigned int i = 0U; i < num_warriors; i++)
    {
      display_insn (i);
    }

    draw_pc_ind (curr_warrior);

    draw_timer (execed_insns);

    Uint32 t0 = SDL_GetTicks ();
    SDL_UpdateRect (screen, 0, 0, scr_width, scr_height);
    Uint32 update_time = SDL_GetTicks () - t0;

    if (paused == false && update_time < min_ms_per_batch)
    {
      SDL_Delay (min_ms_per_batch - update_time);
    }

    queued_updates = 0U;
  }
  else
  {
    queued_updates++;
  }

  if (SDL_MUSTLOCK (screen))
  {
    SDL_UnlockSurface (screen);
  }

  inspecting = false;
  if (warriors[curr_warrior].tasks != NULL)
  {
    inspect_orig_pc = warriors[curr_warrior].tasks->pc;
  }

  /* Process user input, if any. */
  bool proceed = false;
  SDL_Event event;
  if (paused == true)
  {
    while (proceed == false)
    {
      SDL_WaitEvent (&event);
      do
      {
        proceed = sdlui_process_event (&event, &cmd, curr_warrior);
      } while (proceed == false && SDL_PollEvent (&event));
    }
  }
  else
  {
    while (proceed == false && SDL_PollEvent (&event))
    {
      proceed = sdlui_process_event (&event, &cmd, curr_warrior);
    }
  }

  return cmd;
}


/* Initialises the user interface. FULL_SCREEN indicates if the display
   should be shown using the full screen space or in a windowed display.
   Returns 0 on success, a non-zero value on failure. */
int
sdlui_init (bool full_screen)
{
  // Initialise SDL.
  if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1)
  {
    fprintf (stderr, "Could not initialise SDL: %s.\n", SDL_GetError ());
    return 1;
  }

  atexit (SDL_Quit);

  const SDL_VideoInfo *vid_info = SDL_GetVideoInfo ();
  if (vid_info == NULL)
  {
    fprintf (stderr, "Could not get video information: %s.\n",
             SDL_GetError ());
    return 1;
  }

  Uint32 vid_flags = SDL_SWSURFACE;
  if (full_screen == true)
  {
    vid_flags |= SDL_FULLSCREEN;
  }

  screen
    = SDL_SetVideoMode (scr_width, scr_height, vid_info->vfmt->BitsPerPixel,
                        vid_flags);
  if (screen == NULL)
  {
    fprintf (stderr, "Could not set %ux%u %u-bpp video mode: %s.\n",
             scr_width, scr_height, vid_info->vfmt->BitsPerPixel,
             SDL_GetError ());
    return 1;
  }

  scr_bypp = screen->format->BytesPerPixel;

  char tmp_buf[TMP_BUF_SIZE];
  if (num_warriors == 2)
  {
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "ZINC: %s v/s %s", warriors[0].name,
             warriors[1].name);
  }
  else
  {
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "ZINC: %s", warriors[0].name);
  }
  tmp_buf[TMP_BUF_SIZE - 1] = '\0';
  SDL_WM_SetCaption (tmp_buf, NULL);

  SDL_ShowCursor (full_screen == true ? SDL_DISABLE : SDL_ENABLE);
  SDL_EventState (SDL_MOUSEMOTION, SDL_IGNORE);
  SDL_EventState (SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
  SDL_EventState (SDL_MOUSEBUTTONUP, SDL_IGNORE);
  SDL_EnableKeyRepeat (SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

  sdltxt_init (screen->format);
  sdltxt_metrics (&font_width, &font_height);

  bg_clr.r = 0x00;
  bg_clr.g = 0x00;
  bg_clr.b = 0x00;
  bg_clr_num = SDL_MapRGB (screen->format, bg_clr.r, bg_clr.g, bg_clr.b);

  default_clr.r = 0x00;
  default_clr.g = 0xB2;
  default_clr.b = 0xB2;
  default_clr_num = SDL_MapRGB (screen->format, default_clr.r,
                                default_clr.g, default_clr.b);

  border_clr.r = 0xB2;
  border_clr.g = 0xB2;
  border_clr.b = 0x00;
  border_clr_num = SDL_MapRGB (screen->format, border_clr.r,
                               border_clr.g, border_clr.b);

  warrior_clrs[UNKNOWN_WARRIOR].r = 0x53;
  warrior_clrs[UNKNOWN_WARRIOR].g = 0x53;
  warrior_clrs[UNKNOWN_WARRIOR].b = 0x53;
  warrior_clr_nums[UNKNOWN_WARRIOR]
    = SDL_MapRGB (screen->format, warrior_clrs[UNKNOWN_WARRIOR].r,
                  warrior_clrs[UNKNOWN_WARRIOR].g,
                  warrior_clrs[UNKNOWN_WARRIOR].b);

  warrior_clrs[UNKNOWN_WARRIOR + 1].r = 0xB2;
  warrior_clrs[UNKNOWN_WARRIOR + 1].g = 0x00;
  warrior_clrs[UNKNOWN_WARRIOR + 1].b = 0x00;
  warrior_clr_nums[UNKNOWN_WARRIOR + 1]
    = SDL_MapRGB (screen->format, warrior_clrs[UNKNOWN_WARRIOR + 1].r,
                  warrior_clrs[UNKNOWN_WARRIOR + 1].g,
                  warrior_clrs[UNKNOWN_WARRIOR + 1].b);

  warrior_clrs[UNKNOWN_WARRIOR + 2].r = 0x00;
  warrior_clrs[UNKNOWN_WARRIOR + 2].g = 0xB2;
  warrior_clrs[UNKNOWN_WARRIOR + 2].b = 0x00;
  warrior_clr_nums[UNKNOWN_WARRIOR + 2]
    = SDL_MapRGB (screen->format, warrior_clrs[UNKNOWN_WARRIOR + 2].r,
                  warrior_clrs[UNKNOWN_WARRIOR + 2].g,
                  warrior_clrs[UNKNOWN_WARRIOR + 2].b);

  stat_rect.x = gutter_size;
  stat_rect.y = scr_height - gutter_size - font_height;
  stat_rect.w = scr_width - 2 * stat_rect.x;
  stat_rect.h = font_height;
  max_stat_chars = stat_rect.w / font_width;

  core_rect.w = num_x_cells * (cell_size + 2);
  core_rect.h = num_y_cells * (cell_size + 2);
  core_rect.x = (scr_width - core_rect.w) / 2;
  core_rect.y = stat_rect.y - 2 * gutter_size - core_rect.h;

  /* It should take around 10 seconds to execute through 1,00,000 cycles. */
  min_ms_per_batch = MAX_QUEUED_UPDATES * 10U * 1000U / 100000U;
  if (min_ms_per_batch > 1U)
  {
    min_ms_per_batch -= 1U;
  }

  return 0;
}


/* Called at the beginning of a battle. Returns the wish of the user with
   respect to the battle (proceed, quit, etc.). */
user_wish_t
sdlui_start_battle (void)
{
  prev_drawn_pc = INVALID_CELL_ADDR;

  if (SDL_MUSTLOCK (screen))
  {
    if (SDL_LockSurface (screen) < 0)
    {
      return 1;
    }
  }

  /* Clear the region. */
  SDL_FillRect (screen, NULL, bg_clr_num);

  timer_rect.x = core_rect.x - 1U;
  timer_rect.y = core_rect.y - 1U - 4 * gutter_size;
  timer_rect.w = core_rect.w + 2U;
  timer_rect.h = 2 * gutter_size;

  SDL_Rect tmp_rect;
  tmp_rect.x = timer_rect.x - 1U;
  tmp_rect.y = timer_rect.y - 1U;
  tmp_rect.w = timer_rect.w + 2U;
  tmp_rect.h = timer_rect.h + 2U;
  SDL_FillRect (screen, &tmp_rect, border_clr_num);
  tmp_rect.x -= 1U;
  tmp_rect.y -= 1U;
  tmp_rect.w += 2U;
  tmp_rect.h += 2U;
  SDL_FillRect (screen, &tmp_rect, border_clr_num);

  /* Print general information. */
  char tmp_buf[TMP_BUF_SIZE];
  snprintf (tmp_buf, TMP_BUF_SIZE - 1, "ZINC v%s", zinc_version);
  tmp_buf[TMP_BUF_SIZE - 1] = '\0';
  Uint16 x = (scr_width - strlen (tmp_buf) * font_width) / 2;
  Uint16 y = gutter_size;
  sdltxt_write (tmp_buf, strlen (tmp_buf), screen, x, y);
  draw_x_line (x, y + font_height, strlen (tmp_buf) * font_width,
               default_clr_num);
  draw_x_line (x, y + font_height + 1U, strlen (tmp_buf) * font_width,
               default_clr_num);

  write_status (paused_stat_msg);

  /* Point out the information about the warriors. */
  int max_chars = (scr_width / 2 - 2 * gutter_size) / font_width;

  for (int i = 0; i < num_warriors; i++)
  {
    x = gutter_size + i * (scr_width / 2 - 2 * gutter_size);
    y = core_rect.y - 10 * gutter_size - 5 * font_height;

    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "Warrior: %s %s", warriors[i].name,
             (warriors[i].version == NULL) ? "" : warriors[i].version);
    tmp_buf[TMP_BUF_SIZE - 1] = '\0';
    sdltxt_write (tmp_buf, max_chars, screen, x, y);

    y += gutter_size + font_height;
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "Author:  %s",
             (warriors[i].author == NULL) ? "" : warriors[i].author);
    tmp_buf[TMP_BUF_SIZE - 1] = '\0';
    sdltxt_write (tmp_buf, max_chars, screen, x, y);

    y += gutter_size + font_height;
    sdltxt_write ("Colour:  ", max_chars, screen, x, y);
    SDL_Rect col_rect;
    col_rect.x = x + 9 * font_width;
    col_rect.y = y;
    col_rect.w = 10 * font_width;
    col_rect.h = font_height;
    SDL_FillRect (screen, &col_rect, warrior_clr_nums[warriors[i].id]);
  }

  display_scores ();

  /* Draw the border for the core. */
  Uint16 w = core_rect.w + 4;
  Uint16 h = core_rect.h + 4;
  x = core_rect.x - 2;
  y = core_rect.y - 2;
  draw_box (x, y, w, h, border_clr_num);
  w += 2U;
  h += 2U;
  x -= 1U;
  y -= 1U;
  draw_box (x, y, w, h, border_clr_num);

  /* Draw the core. */
  for (cell_addr_t c = 0U; c < core_size; c++)
  {
    draw_cell (c);
  }

  if (SDL_MUSTLOCK (screen))
  {
    SDL_UnlockSurface (screen);
  }

  paused = true;

  return sdlui_update_battle (0U, INVALID_CELL_ADDR, 0U);
}


/* Called at the end of a battle to indicate the status STATUS (warrior1
   or warrior2 killed, timed out, internal error, etc.) of the battle.
   END_WARRIOR is the warrior on whose PC the simulation ended.
   Returns the wish of the user (reload warriors, quit, etc.). */
user_wish_t
sdlui_finish_battle (battle_status_t status, unsigned int end_warrior)
{
  user_wish_t cmd = RELOAD_WARRIORS;

  display_scores ();
  for (unsigned int i = 0U; i < num_warriors; i++)
  {
    display_insn (i);
  }

  draw_pc_ind (end_warrior);

  char tmp_buf[TMP_BUF_SIZE];
  switch (status)
  {
  case WARRIOR_1_KILLED:
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "\"%s\" was killed",
              warriors[0].name);
    break;

  case WARRIOR_2_KILLED:
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "\"%s\" was killed",
              warriors[1].name);
    break;

  case CYCLES_EXHAUSTED:
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "Timed out");
    break;

  case USER_INTERRUPTED:
    return RELOAD_WARRIORS;
    break;

  case ZINC_FUBARED:
  default:
    snprintf (tmp_buf, TMP_BUF_SIZE - 1, "*** Internal Error ***");
    break;
  }

  tmp_buf[TMP_BUF_SIZE - 1] = '\0';
  int tmp_len = strlen (tmp_buf);
  if (tmp_len < (TMP_BUF_SIZE - 1))
  {
    const char *app_str = " (R = Reload, ESC = Quit)";
    if (strlen (app_str) < (TMP_BUF_SIZE - tmp_len - 1))
    {
      strcat (tmp_buf, " (R = Reload, ESC = Quit)");
    }
  }
  write_status (tmp_buf);
  
  SDL_UpdateRect (screen, 0, 0, scr_width, scr_height);

  paused = true;
  inspecting = false;
  if (warriors[end_warrior].tasks != NULL)
  {
    inspect_orig_pc = warriors[end_warrior].tasks->pc;
  }

  SDL_Event event;
  bool proceed = false;
  while (proceed == false)
  {
    SDL_WaitEvent (&event);
    do
    {
      proceed = sdlui_process_event (&event, &cmd, end_warrior);
    } while (proceed == false && SDL_PollEvent (&event));
  }

  return cmd;
}


/* Quits the graphical interface and frees associated resources. Returns 0
   on success, a non-zero value otherwise. */
int
sdlui_quit (void)
{
  int error = 0;
  sdltxt_quit ();
  return error;
}
