/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The main module.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* This should not have been necessary in this module, but the main method
   might have to be renamed on some platforms (e.g. Win32) by SDL. */
#include <SDL.h>

#include "zinc.h"
#include "zasm.h"
#include "exec.h"
#include "sdlui.h"
#include "dump.h"

/* The current version of ZINC. */
const char *zinc_version = "0.1";

/* The size of the core. */
unsigned int core_size = DEFAULT_CORE_SIZE;

/* The core. */
cell_t *core = NULL;

/* The minimum guaranteed number of cells separating two programmes when
   they are loaded into the core. */
unsigned int min_prog_separation = DEFAULT_MIN_PROG_SEP;

/* The number of warriors that have been loaded into the core. */
unsigned int num_warriors = 0U;

/* The warrior programmes. */
warrior_t warriors[MAX_WARRIORS];

/* The maximum number of cycles for which to run the simulator. */
unsigned int max_cycles = DEFAULT_MAX_CYCLES;

/* The maximum number of assembled instructions allowed in a programme. */
unsigned int max_prog_insns = DEFAULT_MAX_PROG_INSNS;

/* The maximum number of tasks allowed for a single programme. */
unsigned int max_prog_tasks = DEFAULT_MAX_PROG_TASKS;

/* Flag that indicates whether the GUI should be shown or not. If this is
   set to true, GUI routines should *not* be called. */
bool opt_no_gui = false;

/* Flag that indicates whether the GUI should run in full-screen or
   windowed mode. */
static bool opt_full_screen = false;

/* Flag that indicates whether we should display the programmes as seen
   by the loader after assembly. */
static bool opt_dump_progs = false;

/* The maximum number of battles to run in non-interactive mode. */
static unsigned int max_ni_battles = 10;


/* Prints out the usage of the programme as well as a short copyright
   notice. PROG_NAME is what the programme should call itself. */
static void
print_usage (const char *prog_name)
{
  printf ("ZINC version %s\n", zinc_version);
  printf ("Copyright (C) 2006 Ranjit Mathew.\n");
  printf ("\n");
  printf ("Usage: %s [options] file1 [file2]\n", prog_name);
  printf ("Options:\n");
  printf ("  -c \tUse command-line interface (no GUI).\n");
  printf ("  -d \tDump compiled programmes to stdout and exit.\n");
  printf ("  -f \tRun full-screen.\n");
  printf ("  -s \tAllow only a single task per programme.\n");
  printf ("\n");
  printf ("Send bug reports to rmathew@gmail.com.\n");
}


/* Processes command-line arguments. ARGC holds the number of arguments
   and ARGV points to the arguments. Returns 0 on success, 1 otherwise. */
static int
process_args (int argc, char *argv[])
{
  int i, error = 0;
  int warrior_count = 0;

  for (i = 1; i < argc; i++)
  {
    char *an_arg = argv[i];

    if (an_arg[0] == '-')
    {
      switch (an_arg[1])
      {
      case 'c':
        opt_no_gui = true;
        break;

      case 'd':
        opt_dump_progs = true;
        break;

      case 'f':
        opt_full_screen = true;
        break;

      case 's':
        max_prog_tasks = 1U;
        break;

      case '\0':
        fprintf (stderr, "ERROR: Missing option letter.\n\n");
        error = 1;
        break;

      default:
        fprintf (stderr, "ERROR: Unknown option letter \"%c\".\n\n",
                 an_arg[1]);
        error = 1;
        break;
      }
    }
    else if (warrior_count < MAX_WARRIORS)
    {
      warriors[warrior_count].file = an_arg;
      warrior_count++;
    }
    else
    {
      fprintf (stderr, "ERROR: Extra argument \"%s\".\n\n", an_arg);
      error = 1;
    }
  }

  if (warrior_count == 0)
  {
    fprintf (stderr, "ERROR: No warrior programme specified.\n\n");
    error = 1;
  }

  num_warriors = warrior_count;

  return error;
}


/* Normalise the given integral value N according to the modulo arithmetic
   rules. */
cell_addr_t
normalise (int32_t n)
{
  cell_addr_t ret_val = 0U;

  if (n == 0)
  {
    ret_val = 0U;
  }
  else if (n < 0)
  {
    while (n < 0)
    {
      n += core_size;
    }

    ret_val = n;
  }
  else
  {
    ret_val = (n % core_size);
  }

  return ret_val;
}


/* Loads the assembled warrior programmes into the core. */
static void
load_warriors (void)
{
  unsigned int i, j;
  cell_addr_t avail_range = core_size, prev_addr = 0U;

  /* Initialise the core. Since both OP_DAT and MODE_IMMEDIATE have the
     value 0, the following has the effect of setting all cells to the
     equivalent of "DAT #0". */
  memset (core, 0, core_size * sizeof (cell_t));

  /* Load the warriors into the core. */
  for (i = 0U; i < num_warriors; i++)
  {
    cell_addr_t start_addr
      = ((prev_addr + max_prog_insns) + (rand () % avail_range)) % core_size;

    prev_addr = start_addr;
    avail_range -= (2 * max_prog_insns);

    task_t *task = (task_t *)malloc (sizeof (task_t));
    task->pc = (start_addr + warriors[i].init_pc) % core_size;
    task->next = task;

    warriors[i].alive = true;
    warriors[i].num_tasks = 1U;
    warriors[i].tasks = task;

    for (j = 0U; j < warriors[i].num_insns; j++)
    {
      cell_addr_t addr = (start_addr + j) % core_size;

      core[addr].marker = warriors[i].id;
      core[addr].op_code = warriors[i].insns[j].op_code;
      core[addr].mode_a = warriors[i].insns[j].mode_a;
      core[addr].mode_b = warriors[i].insns[j].mode_b;
      core[addr].op_a = warriors[i].insns[j].op_a;
      core[addr].op_b = warriors[i].insns[j].op_b;
    }
  }
}


/* The entry point into the programme. ARGC holds the number of arguments
   on the command line and ARGV points to them. Returns 0 on success and
   1 on failure. */
int
main (int argc, char *argv[])
{
  if (argc < 2)
  {
    print_usage (argv[0]);
    return EXIT_SUCCESS;
  }

  for (int i = 0; i < MAX_WARRIORS; i++)
  {
    warriors[i].id = UNKNOWN_WARRIOR + i + 1;
    warriors[i].alive = true;
    warriors[i].file = NULL;
    warriors[i].name = NULL;
    warriors[i].version = NULL;
    warriors[i].author = NULL;
    warriors[i].num_insns = 0U;
    warriors[i].insns = NULL;
    warriors[i].init_pc = 0U;
    warriors[i].num_tasks = 0U;
    warriors[i].tasks = NULL;
    warriors[i].score = 0U;
  }

  if (process_args (argc, argv) != 0)
  {
    print_usage (argv[0]);
    return EXIT_FAILURE;
  }

  for (int i = 0; i < num_warriors; i++)
  {
    if (warriors[i].file != NULL)
    {
      if (assemble_warrior (&warriors[i]) != 0)
      {
        return EXIT_FAILURE;
      }
      else
      {
        if (warriors[i].name == NULL)
        {
          warriors[i].name = (char *)malloc( 10 * sizeof (char));
          snprintf (warriors[i].name, 10, "Warrior%d", warriors[i].id);
        }

        if (opt_dump_progs == true)
        {
          dump_warrior (&warriors[i]);
        }
      }
    }
  }

  if (opt_dump_progs == true)
  {
    return EXIT_SUCCESS;
  }

  core = (cell_t *)malloc (core_size * sizeof (cell_t));
  if (core == NULL)
  {
    fprintf (stderr, "ERROR: Unable to allocate memory for core.\n\n");
    return EXIT_FAILURE;
  }

  /* Note that because of the values of the op codes and the addressing
     modes, the calloc() has initialised the core to the equivalent of
     "DAT #0". */

  /* Set a seed for the random number generator. */
  srand (time (NULL));

  if (opt_no_gui == false)
  {
    if (sdlui_init (opt_full_screen) != 0)
    {
      return EXIT_FAILURE;
    }
  }
  else
  {
    printf ("Battle Results:\n");
  }

  unsigned int num_battles = 0U;
  user_wish_t cmd = RELOAD_WARRIORS;
  while (cmd == RELOAD_WARRIORS)
  {
    battle_status_t status = ZINC_FUBARED;
    unsigned int end_warrior = 0U;

    load_warriors ();

    if (opt_no_gui == false)
    {
      cmd = sdlui_start_battle ();
    }
    else
    {
      cmd = CONTINUE_BATTLE;
    }

    if (cmd == CONTINUE_BATTLE)
    {
      if (exec_battle (&status, &cmd, &end_warrior) != 0)
      {
        return EXIT_FAILURE;
      }
    }
    else if (cmd == RELOAD_WARRIORS)
    {
      continue;
    }

    if (cmd != QUIT_ZINC)
    {
      if (opt_no_gui == false)
      {
        cmd = sdlui_finish_battle (status, end_warrior);
      }
      else
      {
        cmd
          = (num_battles == (max_ni_battles - 1)) ? QUIT_ZINC
            : RELOAD_WARRIORS;
        printf ("%4u. ", num_battles);
        switch (status)
        {
        case WARRIOR_1_KILLED:
          printf ("\"%s\" was killed.\n", warriors[0].name);
          break;

        case WARRIOR_2_KILLED:
          printf ("\"%s\" was killed.\n", warriors[1].name);
          break;

        case CYCLES_EXHAUSTED:
          printf ("Timed out.\n");
          break;

        case USER_INTERRUPTED:
          printf ("User interrupted.\n");
          break;

        case ZINC_FUBARED:
        default:
          fprintf (stderr, "** Internal Error ** \n");
          break;
        }
      }
    }

    num_battles++;
  }

  if (opt_no_gui == false)
  {
    if (sdlui_quit () != 0)
    {
      return EXIT_FAILURE;
    }
  }
  else
  {
    /* Print out the final scores. */
    printf ("\nFinal Scores:\n");
    for (int i = 0; i < num_warriors; i++)
    {
      printf ("    \"%s\" - %u\n", warriors[i].name, warriors[i].score);
    }
  }

  return EXIT_SUCCESS;
}
