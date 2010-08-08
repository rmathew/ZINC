/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The ZINC interpreter.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zinc.h"
#include "exec.h"
#include "sdlui.h"

/* Clamp the value of X to within 0 to CORE_SIZE - 1. The leaving
   out of the normal parentheses protecting X in macros is intentional,
   as is the leaving out of the terminating semi-colon. */
#define CLAMP_VAL(x) \
  while (x >= core_size) x -= core_size

static unsigned int execed_insns;

static cell_t tmp_cell;


/* Kills the current task of the warrior at index IDX. If this was the last
   task in the warrior's tasks queue, declare the warrior dead by returning
   TRUE, else return FALSE. */
static bool
kill_curr_task (unsigned int idx)
{
  bool kill_warrior = false;

  if (warriors[idx].num_tasks > 0U)
  {
    warriors[idx].num_tasks -= 1U;
  }
  else
  {
    fprintf (stderr,
             "Internal error (0 tasks for warrior #%u) in kill_curr_task.\n",
             idx);

    return true;
  }

  if (warriors[idx].tasks->next == warriors[idx].tasks)
  {
    /* This was the last remaining task in the warrior's task queue. This
       warrior programme is dead. */

    // Note: We do not invalidate the task pointer as it is needed for
    // showing to the user the last instruction that faulted.
    //
    // warriors[idx].tasks->pc = INVALID_CELL_ADDR;
    // free (warriors[idx].tasks);
    // warriors[idx].tasks = NULL;

    warriors[idx].tasks->next = NULL;
    warriors[idx].alive = false;
    kill_warrior = true;
  }
  else
  {
    /* Kill this task.
       Since the task queue is a singly-linked circular list, we "remove"
       the current node by copying over the data of the next node and
       then removing that node. This is a favourite question for many an
       interviewer. */

    task_t *tmp_task_ptr = warriors[idx].tasks->next;
    warriors[idx].tasks->pc = tmp_task_ptr->pc;
    warriors[idx].tasks->next = tmp_task_ptr->next;

    tmp_task_ptr->pc = INVALID_CELL_ADDR;
    tmp_task_ptr->next = NULL;
    free (tmp_task_ptr);
  }

  return kill_warrior;
}


/* Updates the scores at the end of a battle. If a warrior is alive at
   end of a battle, it is awarded (W^2 - 1)/S points, where W is the
   total number of warriors and S is the number of warriors that survived
   the battle. */
static void
update_scores (void)
{
  unsigned int num_survivors = 0;

  for (unsigned int i = 0; i < num_warriors; i++)
  {
    if (warriors[i].alive == true)
    {
      num_survivors++;
    }
  }

  uint32_t points
    = (num_survivors == 0U) ? 0U
      : (num_warriors * num_warriors - 1U) / num_survivors;

  for (unsigned int i = 0; i < num_warriors; i++)
  {
    if (warriors[i].alive == true)
    {
      warriors[i].score += points;
    }
  }
}


/* Gets the operand for the operand field value OP and addressing mode
   MODE for an instruction located at PC. Returns a pointer to the intended
   cell and the address of the intended cell in ADDR. */
static cell_t *
get_operand (uint8_t mode, cell_addr_t op, cell_addr_t pc, cell_addr_t *addr)
{
  cell_t *ret_val = NULL;

  switch (mode)
  {
  case MODE_IMMEDIATE:
    memset (&tmp_cell, 0, sizeof (cell_t));
    tmp_cell.op_b = op;
    *addr = op;
    ret_val = &tmp_cell;
    break;

  case MODE_DIRECT:
    *addr = (pc + op);
    CLAMP_VAL (*addr);
    ret_val = &core[*addr];
    break;

  case MODE_INDIRECT:
    pc = (pc + op);
    CLAMP_VAL (pc);
    *addr = pc + core[pc].op_b;
    CLAMP_VAL (*addr);
    ret_val = &core[*addr];
    break;

  default:
    fprintf (stderr, "Internal Error (invalid mode %u) in fetch_operand.\n",
             mode);
    break;
  }

  return ret_val;
}


/* Executes a battle. Returns the error code, the status of the battle in
   STATUS, the user's wish in CMD and the warrior on whose task the simulation
   ended in END_WARRIOR. */
int
exec_battle (battle_status_t *status, user_wish_t *cmd,
             unsigned int *end_warrior)
{
  int error = 0;

  cell_addr_t mod_cell;

  unsigned int alive_warriors = num_warriors;
  unsigned int curr_warrior = 0U;
  *end_warrior = curr_warrior;
  execed_insns = 0U;
  while (execed_insns < max_cycles && *cmd == CONTINUE_BATTLE)
  {
    mod_cell = INVALID_CELL_ADDR;

    if (warriors[curr_warrior].tasks != NULL)
    {
      cell_t *cell;
      cell_t *op1, *op2;
      cell_addr_t addr_A, addr_B;
      cell_addr_t val_A, val_B;
      bool kill_warrior;

      // Decode the instruction at the cell pointed to by the PC of the
      // current task of the current warrior.
      //
      // We use a giant switch statement to figure out what to do for a given
      // opcode. An alternative would have been to use a table of pointers to
      // decoder functions.

      cell = &core[warriors[curr_warrior].tasks->pc];
      switch (cell->op_code)
      {
      case OP_DAT:
        kill_warrior = kill_curr_task (curr_warrior);
        if (kill_warrior == true)
        {
          alive_warriors -= 1U;

          /* If there was only a single loaded warrior and it is killed or
             if there were multiple loaded warriors and now only one is
             alive, we need to end the simulation. */
          if (alive_warriors == 0U || alive_warriors == 1U)
          {
            *cmd = RELOAD_WARRIORS;
          }

          /* FIXME: Assumes only two warriors. */
          *status
            = (curr_warrior == 0U) ? WARRIOR_1_KILLED : WARRIOR_2_KILLED;
        }
        break;
      
      case OP_MOV:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        memcpy (op2, op1, sizeof (cell_t));
        op2->marker = warriors[curr_warrior].id;
        mod_cell = addr_B;
        warriors[curr_warrior].tasks->pc++;
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_ADD:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        op2->op_b = op1->op_b + op2->op_b;
        CLAMP_VAL (op2->op_b);
        op2->marker = warriors[curr_warrior].id;
        mod_cell = addr_B;
        warriors[curr_warrior].tasks->pc++;
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_SUB:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        op2->op_b = op2->op_b + core_size - op1->op_b;
        CLAMP_VAL (op2->op_b);
        op2->marker = warriors[curr_warrior].id;
        mod_cell = addr_B;
        warriors[curr_warrior].tasks->pc++;
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_MUL:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        op2->op_b
          = (cell_addr_t )((uint32_t )op1->op_b * (uint32_t )op2->op_b);
        CLAMP_VAL (op2->op_b);
        op2->marker = warriors[curr_warrior].id;
        mod_cell = addr_B;
        warriors[curr_warrior].tasks->pc++;
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_DIV:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        if (op1->op_b == 0U)
        {
          alive_warriors -= 1U;

          /* If there was only a single loaded warrior and it is killed or
             if there were multiple loaded warriors and now only one is
             alive, we need to end the simulation. */
          if (alive_warriors == 0U || alive_warriors == 1U)
          {
            *cmd = RELOAD_WARRIORS;
          }

          /* FIXME: Assumes only two warriors. */
          *status
            = (curr_warrior == 0U) ? WARRIOR_1_KILLED : WARRIOR_2_KILLED;
        }
        else
        {
          op2->op_b
            = (cell_addr_t )((uint32_t )op2->op_b / (uint32_t )op1->op_b);
          CLAMP_VAL (op2->op_b);
          op2->marker = warriors[curr_warrior].id;
          mod_cell = addr_B;
          warriors[curr_warrior].tasks->pc++;
          CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        }
        break;

      case OP_MOD:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        if (op1->op_b == 0U)
        {
          alive_warriors -= 1U;

          /* If there was only a single loaded warrior and it is killed or
             if there were multiple loaded warriors and now only one is
             alive, we need to end the simulation. */
          if (alive_warriors == 0U || alive_warriors == 1U)
          {
            *cmd = RELOAD_WARRIORS;
          }

          /* FIXME: Assumes only two warriors. */
          *status
            = (curr_warrior == 0U) ? WARRIOR_1_KILLED : WARRIOR_2_KILLED;
        }
        else
        {
          op2->op_b
            = (cell_addr_t )((uint32_t )op2->op_b % (uint32_t )op1->op_b);
          CLAMP_VAL (op2->op_b);
          op2->marker = warriors[curr_warrior].id;
          mod_cell = addr_B;
          warriors[curr_warrior].tasks->pc++;
          CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        }
        break;

      case OP_JMP:
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        warriors[curr_warrior].tasks->pc = addr_B;
        break;

      case OP_JMZ:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        if (op1->op_b == 0U)
        {
          warriors[curr_warrior].tasks->pc = addr_B;
        }
        else
        {
          warriors[curr_warrior].tasks->pc++;
          CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        }
        break;

      case OP_JMN:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        if (op1->op_b != 0U)
        {
          warriors[curr_warrior].tasks->pc = addr_B;
        }
        else
        {
          warriors[curr_warrior].tasks->pc++;
          CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        }
        break;

      case OP_SKL:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        val_A = op1->op_b;

        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        val_B = op2->op_b;

        if (val_A < val_B)
        {
          warriors[curr_warrior].tasks->pc += 2;
        }
        else
        {
          warriors[curr_warrior].tasks->pc += 1;
        }
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_SKE:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        val_A = op1->op_b;

        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        val_B = op2->op_b;

        if (val_A == val_B)
        {
          warriors[curr_warrior].tasks->pc += 2;
        }
        else
        {
          warriors[curr_warrior].tasks->pc += 1;
        }
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_SKN:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        val_A = op1->op_b;

        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        val_B = op2->op_b;

        if (val_A != val_B)
        {
          warriors[curr_warrior].tasks->pc += 2;
        }
        else
        {
          warriors[curr_warrior].tasks->pc += 1;
        }
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_SKG:
        op1 = get_operand (cell->mode_a, cell->op_a,
                           warriors[curr_warrior].tasks->pc, &addr_A);
        val_A = op1->op_b;

        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        val_B = op2->op_b;

        if (val_A > val_B)
        {
          warriors[curr_warrior].tasks->pc += 2;
        }
        else
        {
          warriors[curr_warrior].tasks->pc += 1;
        }
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);
        break;

      case OP_SPL:
        op2 = get_operand (cell->mode_b, cell->op_b,
                           warriors[curr_warrior].tasks->pc, &addr_B);
        warriors[curr_warrior].tasks->pc++;
        CLAMP_VAL (warriors[curr_warrior].tasks->pc);

        /* SPL creates a new task only if the warrior can afford to have
           more tasks. Note that the new task is added at the end of the
           task queue *after* the task that spawned it. This is a little
           detail that is crucial to the correct operation of many a 
           warrior out there. */
        if (warriors[curr_warrior].num_tasks < max_prog_tasks)
        {
          /* We add the node for the new task after the current node and
             adjust the warriors current task pointer to point to it. This
             ensures that the next instruction will be picked up from the
             task that originally came in the task queue after the task
             that spawned this new task. */
          task_t *task = (task_t *)malloc (sizeof (task_t));
          task->pc = addr_B;
          task->next = warriors[curr_warrior].tasks->next;
          warriors[curr_warrior].tasks->next = task;
          warriors[curr_warrior].tasks = task;
          warriors[curr_warrior].num_tasks += 1U;
        }
        break;

      default:
        fprintf (stderr,
                 "Internal Error (invalid op-code %u) in exec_battle.\n",
                 cell->op_code);
        error = 1;
        *cmd = QUIT_ZINC;
        *status = ZINC_FUBARED;
        break;
      }

      if (*cmd == CONTINUE_BATTLE && warriors[curr_warrior].num_tasks > 1)
      {
        if (warriors[curr_warrior].tasks->next != NULL)
        {
          warriors[curr_warrior].tasks = warriors[curr_warrior].tasks->next;
        }
        else
        {
          fprintf (stderr,
                   "Internal Error (invalid next task) in exec_battle.\n");
          error = 1;
          *cmd = QUIT_ZINC;
          *status = ZINC_FUBARED;
        }
      }
    }

    /* We have executed yet another instruction. */
    execed_insns += 1U;

    /* Tell our caller which was the warrior whose instruction we were
       executing when our simulation ended. */
    *end_warrior = curr_warrior;

    /* Pick up the next eligible warrior from the processes queue. */
    if (*cmd == CONTINUE_BATTLE)
    {
      do
      {
        curr_warrior += 1U;
        if (curr_warrior >= num_warriors)
        {
          curr_warrior = 0U;
        }
      } while (alive_warriors > 0U && warriors[curr_warrior].alive == false);
    }

    /* Update the user interface if the battle is still on. Note that we have
       already moved on to the next warrior. This sequencing is intentional as
       we show the user the instruction that is _about to be_ executed. */
    if (*cmd == CONTINUE_BATTLE)
    {
      if (opt_no_gui == false)
      {
        *cmd = sdlui_update_battle (curr_warrior, mod_cell, execed_insns);

        if (*cmd != CONTINUE_BATTLE)
        {
          *status = USER_INTERRUPTED;
        }
      }
    }
  }

  if (execed_insns == max_cycles)
  {
    *status = CYCLES_EXHAUSTED;
  }

  if (*status != USER_INTERRUPTED && *status != ZINC_FUBARED)
  {
    update_scores ();
  }

  return error;
}
