/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  Methods to dump instructions and programmes.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "zinc.h"
#include "dump.h"

/* Instruction mnemonics. The order corresponds to that of the corresponding
   enumeration in zinc.h. */
static char *insn_mnemonics[] =
{
  "DAT",
  "MOV",
  "ADD",
  "SUB",
  "MUL",
  "DIV",
  "MOD",
  "JMP",
  "JMZ",
  "JMN",
  "SKL",
  "SKE",
  "SKN",
  "SKG",
  "SPL",
};

/* The expected number of operands for each of the above instructions.
   Note that the order has to remain the same as in the previous list. */
static int insn_num_ops[] =
{
  1, /* DAT */
  2, /* MOV */
  2, /* ADD */
  2, /* SUB */
  2, /* MUL */
  2, /* DIV */
  2, /* MOD */
  1, /* JMP */
  2, /* JMZ */
  2, /* JMN */
  2, /* SKL */
  2, /* SKE */
  2, /* SKN */
  2, /* SKG */
  1, /* SPL */
};

/* Addressing mode markers. The order corresponds to that of the
   corresponding enumeration in zinc.h. */
static char *mode_markers[] = { "#", "$", "@"};

/* Dumps an instruction at the cell C into the buffer BUF. */
void
dump_insn (char *buf, size_t buf_size, cell_t *c)
{
  snprintf (buf, buf_size, "%s", insn_mnemonics[c->op_code]);
  buf[buf_size - 1] = '\0';

  size_t at = strlen (buf);

  if (at == buf_size)
  {
    return;
  }
  else
  {
    buf_size -= at;
  }

  switch (insn_num_ops[c->op_code])
  {
  case 0:
    /* Nothing else to do. */
    break;

  case 1:
    /* Only op_b is used for such instructions. */
    snprintf (buf + at, buf_size, " %s%d", mode_markers[c->mode_b], c->op_b);
    break;

  case 2:
    snprintf (buf + at, buf_size, " %s%d, %s%d", mode_markers[c->mode_a],
              c->op_a, mode_markers[c->mode_b], c->op_b);
    break;
  }
}


/* Dumps the warrior programme W to standard output. */
void
dump_warrior (warrior_t *w)
{
  printf (";\n; Name:    %s\n", w->name);

  if (w->version != NULL)
  {
    printf ("; Version: %s\n", w->version);
  }

  if (w->author != NULL)
  {
    printf ("; Author:  %s\n", w->author);
  }

  printf (";\n");

  for (unsigned int i = 0U; i < w->num_insns; i++)
  {
    if (i == w->init_pc)
    {
      printf ("START:\n");
    }

    char tmp_buf[TMP_BUF_SIZE];
    dump_insn (tmp_buf, TMP_BUF_SIZE, w->insns + i);
    printf ("  %-20s ; %u\n", tmp_buf, i);
  }

  printf ("\n");
}
