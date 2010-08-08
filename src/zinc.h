/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The primary header file for ZINC.
*/

#ifndef ZINC_H_INCLUDED
#define ZINC_H_INCLUDED

/* The default number of cells in the core. */
#define DEFAULT_CORE_SIZE 8000U

/* The maximum number of cycles for which to run the simulator. */
#define DEFAULT_MAX_CYCLES 100000

/* The maximum number of warriors allowed in the core. */
#define MAX_WARRIORS 2

/* The minimum number of cells separating warrior programmes. */
#define DEFAULT_MIN_PROG_SEP 1000

/* The maximum number of instructions allowed in a warrior programme. */
#define DEFAULT_MAX_PROG_INSNS 1000

/* The maximum number of tasks allowed for a single warrior programme. */
#define DEFAULT_MAX_PROG_TASKS 4000

/* The maximum number of characters allowed in a warrior programme line. */
#define MAX_LINE_LEN 255

/* The maximum number of characters allowed in an identifier or string. */
#define MAX_STR_IDENT_LEN 127

/* The maximum number of decimal digits allowed in a number. */
#define MAX_NUMBER_LEN 5

/* The identifier for an unknown warrior. */
#define UNKNOWN_WARRIOR 0U

/* The size of temporary buffers used for writing messages. */
#define TMP_BUF_SIZE 128

/* The operation codes for the instructions. */
enum
{
  OP_DAT = 0, /* The default instruction; must be zero. */
  OP_MOV,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_JMP,
  OP_JMZ,
  OP_JMN,
  OP_SKL,
  OP_SKE,
  OP_SKN,
  OP_SKG,
  OP_SPL,
};

/* The addressing modes for the operands. */
enum
{
  MODE_IMMEDIATE = 0,      /* '#', default mode; must be zero.  */
  MODE_DIRECT,             /* '$' */
  MODE_INDIRECT,           /* '@' */
};

/* The possible outcomes of a battle or a single-warrior run. */
typedef enum
{
  WARRIOR_1_KILLED,
  WARRIOR_2_KILLED,
  CYCLES_EXHAUSTED,
  USER_INTERRUPTED,
  ZINC_FUBARED,
} battle_status_t;

/* The internal identifer of a warrior. '0' means unknown warrior. */
typedef uint8_t warrior_id_t;

/* The address of a cell in the core. */
typedef uint16_t cell_addr_t;

/* An invalid address for a cell. */
#define INVALID_CELL_ADDR 0xFFFFU

/* The fundamental unit of core - a cell. */
typedef struct cell
{
  /* The identifier of the the last warrior to write into this cell. */
  warrior_id_t marker;

  /* The operation code (opcode) of this instruction. */
  uint8_t op_code;

  /* The addressing mode of the first operand (operand A). */
  uint8_t mode_a;

  /* The addressing mode of the second operand (operand B). */
  uint8_t mode_b;

  /* The first operand (operand A). */
  cell_addr_t op_a;

  /* The second operand (operand B). */
  cell_addr_t op_b;
} cell_t;

/* A task (thread) in a warrior programme. These nodes are maintained in
   a circular linked list; even when there is only a single task left. */
typedef struct task
{
  /* The current programme counter for this task. */
  cell_addr_t pc;

  /* The next task on the task list of the warrior programme. */
  struct task *next;
} task_t;

/* A warrior programme. */
typedef struct warrior
{
  /* The internal identifier for the warrior. */
  warrior_id_t id;

  /* Indicates whether the warrior is alive or not. */
  bool alive;

  /* The path to the file containing the warrior programme. */
  char *file;

  /* The name of the warrior. This will not be NULL. */
  char *name;
  
  /* The version of the warrior programme, if known. */
  char *version;

  /* The author of the warrior programme, if known. */
  char *author;

  /* The number of instructions in the assembled warrior programme. */
  unsigned int num_insns;

  /* The instructions in the assembled warrior programme. */
  cell_t *insns;

  /* The offset of the instruction in the warrior programme to begin
     execution at. */
  cell_addr_t init_pc;

  /* The current number of tasks executing for this warrior programme. */
  unsigned int num_tasks;

  /* The task list for the warrior programme. This pointer always points
     at the current node in the circular task list, or NULL if the warrior
     has died. */
  task_t *tasks;

  /* The score accumulated by the warrior so far. */
  uint32_t score;
} warrior_t;

/* Commands given by the user before, during or after a battle. */
typedef enum
{
  CONTINUE_BATTLE,
  RELOAD_WARRIORS,
  QUIT_ZINC,
} user_wish_t;


/* The version of ZINC. */
extern const char *zinc_version;

/* The number of cells in the core. */
extern unsigned int core_size;

/* The maximum number of cycles for which to run the simulator. */
extern unsigned int max_cycles;

/* The maximum number of instructions allowed for a single programme
   when it is loaded into the core. */
extern unsigned int max_prog_insns;

/* The minimum number of instructions separating two warrior programmes
   when they are loaded into the core. */
extern unsigned int min_prog_separation;

/* The maximum number of tasks allowed per warrior programme. */
extern unsigned int max_prog_tasks;

/* The number of loaded warrior programmes. */
extern unsigned int num_warriors;

/* The loaded warrior programmes. */
extern warrior_t warriors[];

/* The core. */
extern cell_t *core;

/* Whether to show the GUI or just use the command-line interface. */
extern bool opt_no_gui;

extern cell_addr_t normalise (int32_t n);

#endif /* ZINC_H_INCLUDED */
