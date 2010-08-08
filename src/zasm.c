/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The ZINC assembler module.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "zinc.h"
#include "zasm.h"
#include "expr.h"
#include "sym.h"

/* Used to indicate EOF. */
#define NO_MORE_INPUT  -1

/* Used to indicate a line that's too long. */
#define LINE_TOO_LONG  -2

/* The path to the current file being assembled. */
static const char *curr_file = NULL;

/* The current line being assembled. */
static char curr_line[MAX_LINE_LEN + 1];

/* The current line number. */
static line_t line_num = 0;

/* Token values returned by the lexical analyser. */
typedef enum
{
  TK_DAT,
  TK_MOV,
  TK_ADD,
  TK_SUB,
  TK_MUL,
  TK_DIV,
  TK_MOD,
  TK_JMP,
  TK_JMZ,
  TK_JMN,
  TK_SKL,
  TK_SKE,
  TK_SKN,
  TK_SKG,
  TK_SPL,
  TK_ORG,
  TK_NAM,
  TK_VER,
  TK_AUT,
  TK_DEF,
  TK_NUMBER,
  TK_IDENTIFIER,
  TK_STRING,
  TK_LEFT_PAREN,
  TK_RIGHT_PAREN,
  TK_PLUS,
  TK_MINUS,
  TK_ASTERISK,
  TK_SLASH,
  TK_PERCENT,
  TK_COMMA,
  TK_EQUAL,
  TK_COLON,
  TK_HASH,
  TK_DOLLAR,
  TK_AT,
  TK_EOL,
  TK_INVALID,
} token_val_t;

/* A token returned by the lexical analyser. */
typedef struct token
{
  /* The value of the token. */
  token_val_t tk_val;

  /* The starting column on the input line for the token. */
  col_t tk_begin;

  /* The identifier associated with the token, if any. */
  char ident[MAX_STR_IDENT_LEN + 1];

  /* The numeric value associated with the token, if any. */
  cell_addr_t num_val;
} token_t;

#include "keyword.h"

/* Represents a parsing context. */
typedef struct parse_ctx
{
  /* The input buffer. */
  const char *buf;

  /* The current position within the input buffer. */
  col_t *pos;

  /* The current token. */
  token_t *tk;

  /* Indicates whether there has been an error while parsing. */
  bool error;
} parse_ctx_t;

/* Type of an argument expected by an instruction. */
typedef enum
{
  /* There should be no arguments to the instruction. */
  ARG_NONE,

  /* The argument should only be an immediate value. */
  ARG_IMMEDIATE,

  /* The argument should only be an address of a cell in core. */
  ARG_ADDRESS,

  /* The argument can be either an address or an immediate value, but not
     empty. */
  ARG_ANY,
} arg_t;

/* A node to keep track of assembled instructions in a list. */
typedef struct tmp_insn
{
  unsigned char op_code;
  unsigned char mode_a;
  unsigned char mode_b;
  expr_t *op_a_expr;
  expr_t *op_b_expr;

  struct tmp_insn *next;
} tmp_insn_t;

/* Pointers for maintaining a list of instructions assembled so far. New
   instructions are inserted at the tail end of the list. */
static tmp_insn_t *insns_head = NULL;
static tmp_insn_t *insns_tail = NULL;

/* The expression denoting the offset of the starting instruction for
   warrior programme being assembled. */
static expr_t *start_pc;

/* Tracks the offset of the current instruction within the warrior
   programme. */
static cell_addr_t curr_pc = 0U;


/* Indicates an error in the file being assembled to the user. MSG
   points to the error message and WHERE is the number of the line in
   question. */
void
input_error (const char *msg, line_t where)
{
  fprintf (stderr, "%s:%d: ERROR: %s.\n", curr_file, where, msg);
}


/* Points out an error in the file being assembled to the user by
   reproducing the contents of the line in question and pointing to
   the problematic portion of the line. MSG is the error message to
   be shown to the user; LINE contains the contents of the line in
   question; WHERE is the number of the line; POS is the column number
   of the start of the problematic portion. */
static void
point_error (const char *msg, const char *line, line_t where, col_t pos)
{
  col_t i;

  fprintf (stderr, "%s\n", line);
  for (i = 0; i < pos; i++)
  {
    fprintf (stderr, "-");
  }
  fprintf (stderr, "^\n");

  input_error (msg, where);
}


/* Gets the next line from the file pointed to by FP into the buffer
   pointed to by BUF ensuring that only upto LIMIT characters are read
   in. Does not read in the terminating newline character(s) and ignores
   empty lines. Increments the line number counter as needed. Returns
   the number of characters read in or NO_MORE_INPUT on EOF or LINE_TOO_LONG
   if the line length exceeds LIMIT. */
static int
get_line (FILE *fp, char *buf, int limit)
{
  int ch = EOF, i = 0;

  /* Read in all characters till we see either a newline or an EOF. */
  while (--limit > 0 && (ch = fgetc (fp)) != EOF && ch != '\n')
  {
    buf[i++] = ch;
  }
  buf[i] = '\0';

  /* We were at the end-of-file to begin with. */
  if (i == 0 && ch == EOF)
  {
    return NO_MORE_INPUT;
  }

  /* There are still some unread characters in this line. */
  if (limit == 0 && ch != EOF && ch != '\n')
  {
    while ((ch = fgetc (fp)) != EOF && ch != '\n')
    {
      /* Ignore characters till the end of the line. */
    }

    return LINE_TOO_LONG;
  }

  return i;
}


/* Gets the next lexical token given the parsing context P. */
static void
get_token (parse_ctx_t *p)
{
  char ch = p->buf[*(p->pos)];
  int i;

  /* Discard spaces. */
  while (ch == ' ')
  {
    *(p->pos) = *(p->pos) + 1;
    ch = p->buf[*(p->pos)];
  }

  /* Remember the beginning of the token. */
  p->tk->tk_begin = *(p->pos);

  /* Check for the beginning of a comment or the end of line. */
  if (ch == ';' || ch == '\0')
  {
    p->tk->tk_val = TK_EOL;
    return;
  }

  /* Use the first character of the candidate token to determine what
     kind of token it is. */
  switch (ch)
  {
  case '#':
    p->tk->tk_val = TK_HASH;
    goto advance_pos;

  case '$':
    p->tk->tk_val = TK_DOLLAR;
    goto advance_pos;

  case '@':
    p->tk->tk_val = TK_AT;
    goto advance_pos;

  case '+':
    p->tk->tk_val = TK_PLUS;
    goto advance_pos;

  case '-':
    p->tk->tk_val = TK_MINUS;
    goto advance_pos;

  case '*':
    p->tk->tk_val = TK_ASTERISK;
    goto advance_pos;

  case '/':
    p->tk->tk_val = TK_SLASH;
    goto advance_pos;

  case '%':
    p->tk->tk_val = TK_PERCENT;
    goto advance_pos;

  case ':':
    p->tk->tk_val = TK_COLON;
    goto advance_pos;

  case ',':
    p->tk->tk_val = TK_COMMA;
    goto advance_pos;

  case '=':
    p->tk->tk_val = TK_EQUAL;
    goto advance_pos;

  case '(':
    p->tk->tk_val = TK_LEFT_PAREN;
    goto advance_pos;

  case ')':
    p->tk->tk_val = TK_RIGHT_PAREN;
    goto advance_pos;

  advance_pos:
    *(p->pos) = *(p->pos) + 1;
    break;
  
  case '"':
    /* We are reading in a string. */

    *(p->pos) = *(p->pos) + 1;
    ch = p->buf[*(p->pos)];
    p->tk->tk_val = TK_STRING;
    p->tk->tk_begin = *(p->pos);
    i = 0;

    while ((isalnum (ch) || ch == '_' || ch == '.' || ch == '@'
           || ch == '\'' || ch == ' ') && i <= MAX_STR_IDENT_LEN)
    {
      p->tk->ident[i] = ch;
      i++;
      *(p->pos) = *(p->pos) + 1;
      ch = p->buf[*(p->pos)];
    }

    if (i > MAX_STR_IDENT_LEN)
    {
      point_error ("String too long", curr_line, line_num, p->tk->tk_begin);
      p->tk->tk_val = TK_INVALID;
      p->error = true;
    }
    else if (ch == '\0')
    {
      point_error ("String prematurely terminated", curr_line, line_num,
                   *(p->pos));
      p->tk->tk_val = TK_INVALID;
      p->error = true;
    }
    else if (ch == '"')
    {
      p->tk->ident[i] = '\0';
      *(p->pos) = *(p->pos) + 1;
    }
    else
    {
      point_error ("Illegal character in string", curr_line, line_num,
                   *(p->pos));
      p->tk->tk_val = TK_INVALID;
      p->error = true;
    }
    break;

  default:
    /* We should have either a number or an identifier. */

    if (isdigit (ch))
    {
      /* We have a number. */

      char tmp_buf[MAX_NUMBER_LEN + 1];
      i = 0;

      p->tk->tk_val = TK_NUMBER;
      p->tk->tk_begin = *(p->pos);
      while (isdigit (ch) && i < MAX_NUMBER_LEN)
      {
        tmp_buf[i] = ch;
        i++;
        *(p->pos) = *(p->pos) + 1;
        ch = p->buf[*(p->pos)];
      }

      if (i >= MAX_NUMBER_LEN)
      {
        point_error ("Number too long", curr_line, line_num, p->tk->tk_begin);
        p->tk->tk_val = TK_INVALID;
        p->error = true;
      }
      else
      {
        tmp_buf[i] = '\0';
        p->tk->num_val = atoi (tmp_buf);
      }
    }
    else if (isalpha (ch))
    {
      /* We have an identifier. */

      const struct keyword *k = NULL;
      i = 0;

      p->tk->tk_begin = *(p->pos);

      while ((isalnum (ch) || ch == '_') && i < MAX_STR_IDENT_LEN)
      {
        p->tk->ident[i] = toupper (ch);
        i++;
        *(p->pos) = *(p->pos) + 1;
        ch = p->buf[*(p->pos)];
      }

      if (i > MAX_STR_IDENT_LEN)
      {
        point_error ("Identifier too long", curr_line, line_num,
                     p->tk->tk_begin);
        p->tk->tk_val = TK_INVALID;
        p->error = true;
      }
      else
      {
        p->tk->ident[i] = '\0';
        k = find_keyword (p->tk->ident, i);
        p->tk->tk_val = (k == NULL) ? TK_IDENTIFIER : k->val;
      }
    }
    break;
  }
}


/* Pushes back a lexical token within the parsing context P. */
static void
unget_token (parse_ctx_t *p)
{
  if (p->tk != NULL)
  {
    *(p->pos) = p->tk->tk_begin;
    p->tk->tk_val = TK_INVALID;
  }
}


/* Forward declaration. */
static expr_t *parse_expr (parse_ctx_t *);


/* Parses an expected factor within the parsing context P. Returns an
   expression representing the factor, if successful, else NULL. */
static expr_t *
parse_factor (parse_ctx_t *p)
{
  expr_t *ret_expr = NULL;
  col_t tmp_pos;
  expr_t *tmp_expr = NULL;
  sym_val_t *ident_value;

  get_token (p);

  if (p->error != false)
  {
    return NULL;
  }

  switch (p->tk->tk_val)
  {
  case TK_MINUS:
    tmp_expr = parse_factor (p);
    if (p->error == false && tmp_expr != NULL)
    {
      ret_expr = alloc_expr ();
      ret_expr->type = EXPR_NEGATE;
      ret_expr->u.op[0] = tmp_expr;
    }
    break;

  case TK_LEFT_PAREN:
    tmp_pos = p->tk->tk_begin;
    ret_expr = parse_expr (p);
    if (p->error == false && ret_expr != NULL)
    {
      get_token (p);
      if (p->error != false)
      {
        free_expr (ret_expr);
        ret_expr = NULL;
      }
      else if (p->tk->tk_val != TK_RIGHT_PAREN)
      {
        point_error ("Unmatched left parenthesis", curr_line, line_num,
                     tmp_pos);
        free_expr (ret_expr);
        ret_expr = NULL;
        unget_token (p);
        p->error = true;
      }
    }
    break;

  case TK_NUMBER:
    ret_expr = alloc_expr ();
    ret_expr->type = EXPR_NUMBER;
    ret_expr->u.num_val = p->tk->num_val;
    break;

  case TK_IDENTIFIER:
    ident_value = get_sym (p->tk->ident);
    if (ident_value == NULL)
    {
      ident_value = (sym_val_t *)malloc (sizeof (sym_val_t));
      ident_value->type = SYM_UNDEFINED;
      put_sym (p->tk->ident, ident_value);
    }
    ret_expr = alloc_expr ();
    ret_expr->type = EXPR_IDENTIFIER;
    ret_expr->u.identifier = ident_value->name;
    break;

  case TK_EOL:
    point_error ("Unexpected end of input", curr_line, line_num,
                 p->tk->tk_begin);
    p->error = true;
    break;

  default:
    point_error ("Unexpected input", curr_line, line_num, p->tk->tk_begin);
    unget_token (p);
    p->error = true;
    break;
  }

  if (p->error == false && ret_expr != NULL)
  {
    ret_expr->src_line = line_num;
  }

  return ret_expr;
}


/* Parses the rest of a term within the parsing context P.
   FACTOR1_EXPR is the expression representing the term parsed so far. 
   Returns the expression representing the whole term, if successful, else
   NULL. */
static expr_t *
parse_term_rest (parse_ctx_t *p, expr_t *factor1_expr)
{
  expr_t *ret_expr = factor1_expr;
  expr_t *factor2_expr = NULL;
  expr_type_t expr_type = EXPR_MULTIPLY;

  get_token (p);

  if (p->error != false)
  {
    free_expr (factor1_expr);
    return NULL;
  }

  switch (p->tk->tk_val)
  {
  case TK_ASTERISK:
    expr_type = EXPR_MULTIPLY;
    goto handle_binop1;

  case TK_SLASH:
    expr_type = EXPR_DIVIDE;
    goto handle_binop1;

  case TK_PERCENT:
    expr_type = EXPR_MODULUS;
    goto handle_binop1;

  handle_binop1:
    factor2_expr = parse_factor (p);
    if (p->error != false || factor2_expr == NULL)
    {
      free_expr (factor1_expr);
      ret_expr = NULL;
    }
    else
    {
      ret_expr = alloc_expr ();
      ret_expr->type = expr_type;
      ret_expr->u.op[0] = factor1_expr;
      ret_expr->u.op[1] = factor2_expr;
      ret_expr = parse_term_rest (p, ret_expr);
    }
    break;

  default:
    unget_token (p);
    break;
  }

  return ret_expr;
}


/* Parse a term within the parsing context P. Returns the expression
   representing the term, if successful, else NULL. */
static expr_t *
parse_term (parse_ctx_t *p)
{
  expr_t *ret_expr = NULL;
  expr_t *factor_expr = parse_factor (p);

  if (p->error == false && factor_expr != NULL)
  {
    ret_expr = parse_term_rest (p, factor_expr);
  }

  return ret_expr;
} 


/* Parses the rest of an expression in the parsing context P.
   TERM1_EXPR represents the expression for the expression parsed so far.
   Returns the expression for the whole expression, if successful, else
   NULL. */
static expr_t *
parse_expr_rest (parse_ctx_t *p, expr_t *term1_expr)
{
  expr_t *ret_expr = term1_expr;
  expr_t *term2_expr = NULL;
  expr_type_t expr_type = EXPR_ADD;

  get_token (p);

  if (p->error != false)
  {
    free_expr (term1_expr);
    return NULL;
  }

  switch (p->tk->tk_val)
  {
  case TK_PLUS:
    expr_type = EXPR_ADD;
    goto handle_binop2;

  case TK_MINUS:
    expr_type = EXPR_SUBTRACT;
    goto handle_binop2;

  handle_binop2:
    term2_expr = parse_term (p);
    if (p->error != false || term2_expr == NULL)
    {
      free_expr (term1_expr);
      ret_expr = NULL;
    }
    else
    {
      ret_expr = alloc_expr ();
      ret_expr->type = expr_type;
      ret_expr->u.op[0] = term1_expr;
      ret_expr->u.op[1] = term2_expr;
      ret_expr = parse_expr_rest (p, ret_expr);
    }
    break;

  default:
    unget_token (p);
    break;
  }

  return ret_expr;
}


/* Parses an expression in the parsing context P. Returns the expression
   corresponding to the parsed expression, if successful, else NULL. */
static expr_t *
parse_expr (parse_ctx_t *p)
{
  expr_t *ret_expr = NULL;
  expr_t *term_expr = parse_term (p);

  if (p->error == false && term_expr != NULL)
  {
    ret_expr = parse_expr_rest (p, term_expr);

    if (p->error == false && ret_expr != NULL)
    {
      ret_expr->src_line = line_num;
    }
  }

  return ret_expr;
}


/* Parses an address argument for an instruction in the parsing context P.
   ADDR_MODE would be set to the addressing mode of the argument. Returns
   the expression for the address argument, if successful, else NULL. */
static expr_t *
parse_address (parse_ctx_t *p, unsigned char *addr_mode)
{
  expr_t *ret_expr = NULL;

  get_token (p);
  if (p->error != false)
  {
    return NULL;
  }

  switch (p->tk->tk_val)
  {
  case TK_DOLLAR:
    *addr_mode = MODE_DIRECT;
    break;

  case TK_AT:
    *addr_mode = MODE_INDIRECT;
    break;

  case TK_HASH:
    point_error ("Immediate mode not allowed here", curr_line, line_num,
                 p->tk->tk_begin);
    p->error = true;
    break;
  
  case TK_EOL:
    point_error ("Unexpected end of input", curr_line, line_num,
                 p->tk->tk_begin);
    p->error = true;
    break;

  default:
    point_error ("Missing addressing mode indicator", curr_line, line_num,
                 p->tk->tk_begin);
    p->error = true;
    unget_token (p);
    break;
  }

  ret_expr = parse_factor (p);
  return ret_expr;
}


/* Parses an instruction operand in the parsing context P. ADDR_MODE will
   be set to the addressing mode of the operand. Returns the expression
   for the operand, if successful, else NULL. */
static expr_t *
parse_operand (parse_ctx_t *p, unsigned char *addr_mode)
{
  expr_t *ret_expr = NULL;

  get_token (p);
  if (p->error != false)
  {
    return NULL;
  }

  if (p->tk->tk_val == TK_HASH)
  {
    *addr_mode = MODE_IMMEDIATE;
    ret_expr = parse_factor (p);
  }
  else
  {
    unget_token (p);
    ret_expr = parse_address (p, addr_mode);
  }

  return ret_expr;
}


/* Parses an argument of an instruction in the parsing context P. ARG_TYPE
   indicates the type of the argument to expect. MODE will be set to the
   addressing mode of the argument actually found. Returns the expression
   for the argument, if successful, else NULL. */
static expr_t *
parse_argument (parse_ctx_t *p, arg_t arg_type, unsigned char *mode)
{
  expr_t *ret_expr = NULL;

  switch (arg_type)
  {
  case ARG_NONE:
    /* Nothing to do. */
    break;

  case ARG_IMMEDIATE:
    *mode = MODE_IMMEDIATE;
    get_token (p);
    if (p->error != false)
    {
      return NULL;
    }
    else if (p->tk->tk_val == TK_HASH)
    {
      ret_expr = parse_factor (p);
    }
    else
    {
      point_error ("Expected immediate mode argument", curr_line, line_num,
                   p->tk->tk_begin);
      p->error = true;
    }
    break;

  case ARG_ADDRESS:
    ret_expr = parse_address (p, mode);
    break;

  case ARG_ANY:
    ret_expr = parse_operand (p, mode);
    break;

  default:
    point_error ("Internal Error (Illegal argument type in parse_argument)",
                 curr_line, line_num, p->tk->tk_begin);
    p->error = true;
    break;
  }

  return ret_expr;
}


/* Parses an instruction in the parsing context P. The operation code
   (opcode) of the instruction is indicated by OC and OP_A and OP_B
   indicate the expected argument types. A successfully parsed instruction
   is added to the assembled instructions queue. */
static void
parse_instruction (parse_ctx_t *p, unsigned char oc, arg_t op_a, arg_t op_b)
{
  tmp_insn_t *curr_insn = NULL;

  unsigned char mode_a = MODE_IMMEDIATE;
  unsigned char mode_b = MODE_IMMEDIATE;
  expr_t *op_a_expr = NULL;
  expr_t *op_b_expr = NULL;

  if (op_a != ARG_NONE)
  {
    op_a_expr = parse_argument (p, op_a, &mode_a);

    if (p->error != false)
    {
      return;
    }
  }

  if (op_a != ARG_NONE && op_b != ARG_NONE)
  {
    get_token (p);
    if (p->error != false)
    {
      return;
    }
    else if (p->tk->tk_val != TK_COMMA)
    {
      point_error ("Missing comma", curr_line, line_num, p->tk->tk_begin);
      op_b_expr = NULL;
      unget_token (p);
      p->error = true;
      return;
    }
  }

  if (op_b != ARG_NONE)
  {
    op_b_expr = parse_argument (p, op_b, &mode_b);
    if (p->error != false)
    {
      return;
    }
  }

  curr_insn = (tmp_insn_t *)malloc (sizeof (tmp_insn_t));
  curr_insn->op_code = oc;
  curr_insn->mode_a = mode_a;
  curr_insn->mode_b = mode_b;
  curr_insn->op_a_expr = op_a_expr;
  curr_insn->op_b_expr = op_b_expr;
  curr_insn->next = NULL;

  if (insns_head == NULL)
  {
    insns_head = curr_insn;
  }

  if (insns_tail != NULL)
  {
    insns_tail->next = curr_insn;
  }

  insns_tail = curr_insn;
  
  /* Increment the tracked programme counter. */
  curr_pc++;
}


/* Parse a descriptive assembler directive in the parsing context P. TARGET
   is a pointer to a pointer to the descriptive string, the space for which
   will be allocated by this function. */
static void
parse_descr_directive (parse_ctx_t *p, char **target)
{
  get_token (p);
  if (p->error != false)
  {
    return;
  }
  else if (p->tk->tk_val == TK_STRING)
  {
    if (*target != NULL)
    {
      free (*target);
    }
    *target = (char *)malloc (strlen (p->tk->ident) + 1);
    strcpy (*target, p->tk->ident);
  }
  else
  {
    point_error ("String expected", curr_line, line_num, p->tk->tk_begin);
    p->error = true;
  }
}


/* Installs the pre-defined symbols into the symbol table so that
   warrior programmes can refer to them. */
static void
install_predef_syms (void)
{
  sym_val_t *val;

  val = (sym_val_t *)malloc (sizeof (sym_val_t));
  val->type = SYM_CONSTANT;
  val->u.const_val = core_size;
  put_sym ("CORE_SIZE", val);

  val = (sym_val_t *)malloc (sizeof (sym_val_t));
  val->type = SYM_CONSTANT;
  val->u.const_val = max_prog_insns;
  put_sym ("MAX_INSNS", val);

  val = (sym_val_t *)malloc (sizeof (sym_val_t));
  val->type = SYM_CONSTANT;
  val->u.const_val = max_prog_tasks;
  put_sym ("MAX_TASKS", val);

  val = (sym_val_t *)malloc (sizeof (sym_val_t));
  val->type = SYM_CONSTANT;
  val->u.const_val = max_cycles;
  put_sym ("MAX_CYCLES", val);

  val = (sym_val_t *)malloc (sizeof (sym_val_t));
  val->type = SYM_CONSTANT;
  val->u.const_val = min_prog_separation;
  put_sym ("MIN_DISTANCE", val);
}


/* Implements the first pass of the assembler. The instructions and 
   directives of the warrior programme are parsed and the partially
   assembled instructions are queued up for the next pass. FP is a pointer
   to the warrior programme file. WARRIOR is a pointer to the warrior
   being born. Returns 0 on success, 1 on error. */
static int
do_first_pass (FILE *fp, warrior_t *warrior)
{
  int error = 0;

  line_num = 1;
  int num_chars;
  curr_pc = 0U;

  install_predef_syms ();

  warrior->init_pc = 0U;
  insns_head = insns_tail = NULL;
  start_pc = NULL;

  while ((num_chars = get_line (fp, curr_line, MAX_LINE_LEN + 1))
         != NO_MORE_INPUT)
  {
    if (num_chars == LINE_TOO_LONG)
    {
      input_error ("Line too long", line_num);
      error = 1;
    }
    else
    {
      token_t tok;
      col_t posn = 0U;
      col_t tmp_posn = 0U;

      parse_ctx_t parse_context;

      expr_t *tmp_expr = NULL;
      char *tmp_ident = NULL;

      sym_val_t *sym_value;

      parse_context.buf = curr_line;
      parse_context.pos = &posn;
      parse_context.tk = &tok;
      parse_context.error = false;

      get_token (&parse_context);
      switch (tok.tk_val)
      {
      case TK_DAT:
        parse_instruction (&parse_context, OP_DAT, ARG_NONE, ARG_IMMEDIATE);
        goto end_line;
        break;

      case TK_MOV:
        parse_instruction (&parse_context, OP_MOV, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_ADD:
        parse_instruction (&parse_context, OP_ADD, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_SUB:
        parse_instruction (&parse_context, OP_SUB, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_MUL:
        parse_instruction (&parse_context, OP_MUL, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_DIV:
        parse_instruction (&parse_context, OP_DIV, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_MOD:
        parse_instruction (&parse_context, OP_MOD, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_JMP:
        parse_instruction (&parse_context, OP_JMP, ARG_NONE, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_JMZ:
        parse_instruction (&parse_context, OP_JMZ, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_JMN:
        parse_instruction (&parse_context, OP_JMN, ARG_ANY, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_SKL:
        parse_instruction (&parse_context, OP_SKL, ARG_ANY, ARG_ANY);
        goto end_line;
        break;

      case TK_SKE:
        parse_instruction (&parse_context, OP_SKE, ARG_ANY, ARG_ANY);
        goto end_line;
        break;

      case TK_SKN:
        parse_instruction (&parse_context, OP_SKN, ARG_ANY, ARG_ANY);
        goto end_line;
        break;

      case TK_SKG:
        parse_instruction (&parse_context, OP_SKG, ARG_ANY, ARG_ANY);
        goto end_line;
        break;

      case TK_SPL:
        parse_instruction (&parse_context, OP_SPL, ARG_NONE, ARG_ADDRESS);
        goto end_line;
        break;

      case TK_ORG:
        tmp_expr = parse_expr (&parse_context);
        if (tmp_expr != NULL)
        {
          start_pc = tmp_expr;
        }
        goto end_line;
        break;

      case TK_NAM:
        parse_descr_directive (&parse_context, &(warrior->name));
        goto end_line;
        break;

      case TK_VER:
        parse_descr_directive (&parse_context, &(warrior->version));
        goto end_line;
        break;

      case TK_AUT:
        parse_descr_directive (&parse_context, &(warrior->author));
        goto end_line;
        break;

      case TK_DEF:
        get_token (&parse_context);
        if (tok.tk_val == TK_IDENTIFIER)
        {
          tmp_ident = (char *)malloc (strlen (tok.ident) + 1);
          strcpy (tmp_ident, tok.ident);
          tmp_posn = tok.tk_begin;
          get_token (&parse_context);
          if (tok.tk_val == TK_EQUAL)
          {
            tmp_expr = parse_expr (&parse_context);
            if (tmp_expr != NULL)
            {
              sym_value = get_sym (tmp_ident);
              if (sym_value == NULL)
              {
                sym_value = (sym_val_t *)malloc (sizeof (sym_val_t));
                sym_value->type = SYM_EXPR;
                sym_value->u.expr = tmp_expr;
                put_sym (tmp_ident, sym_value);
              }
              else if (sym_value->type == SYM_UNDEFINED)
              {
                /* This means that this symbol was referred to earlier. We
                   do not allow forward references to identifiers defined 
                   using definition directives. Otherwise the user would
                   be able to define circular definitions involving two or
                   more identifiers like:

                     def foo = bar
                     def bar = foo
                   
                   There ought to be a more sophisticated and user-friendly
                   way of dealing with this, but this should do for now. */

                point_error ("Identifier defined too late", curr_line,
                             line_num, tmp_posn);
                error = 1;
                free_expr (tmp_expr);
              }
              else
              {
                point_error ("Identifier redefined", curr_line, line_num,
                             tmp_posn);
                error = 1;
                free_expr (tmp_expr);
              }
            }
          }
          else
          {
            point_error ("'=' expected", curr_line, line_num, tok.tk_begin);
            error = 1;
          }

          free (tmp_ident);
        }
        else
        {
          point_error ("Identifer expected", curr_line, line_num,
                       tok.tk_begin);
          error = 1;
        }
        goto end_line;
        break;

      case TK_IDENTIFIER:
        sym_value = get_sym (tok.ident);
        if (sym_value == NULL)
        {
          sym_value = (sym_val_t *)malloc (sizeof (sym_val_t));
          sym_value->type = SYM_LABEL;
          sym_value->u.const_val = curr_pc;
          put_sym (tok.ident, sym_value);
        }
        else if (sym_value->type == SYM_UNDEFINED)
        {
          sym_value->type = SYM_LABEL;
          sym_value->u.const_val = curr_pc;
        }
        else
        {
          point_error ("Label redefined", curr_line, line_num, tok.tk_begin);
          error = 1;
        }

        get_token (&parse_context);
        if (tok.tk_val != TK_COLON)
        {
          point_error ("Missing colon", curr_line, line_num, tok.tk_begin);
          error = 1;
        }
        goto end_line;
        break;

      case TK_EOL:
        /* Either a comment or an empty line. */
        break;

      end_line:
        if (parse_context.error == false)
        {
          get_token (&parse_context);
          if (parse_context.error == false)
          {
            if (tok.tk_val != TK_EOL)
            {
              point_error ("Extra text on line", curr_line, line_num,
                           tok.tk_begin);
              error = 1;
            }
          }
          else
          {
            error = 1;
          }
        }
        else
        {
          error = 1;
        }
        break;

      default:
        point_error ("Unexpected token", curr_line, line_num, tok.tk_begin);
        error = 1;
        break;
      }
    }

    line_num++;
  }

  curr_line[0] = '\0';

  return error;
}


/* Implements the second pass of the assembler. The partially assembled
   instructions from the first pass are fully assembled (forward references
   resolved, operand expressions evaluated, etc.) and the warrior programme
   created and ready to be loaded into the core. WARRIOR is a pointer to
   the warrior programme being born. Returns 0 on success, 1 on failure. */
static int
do_second_pass (warrior_t *warrior)
{
  int error = 0;

  unsigned int num_insns = curr_pc;

  if (num_insns == 0)
  {
    fprintf (stderr, "%s: ERROR: No instructions in programme.\n", curr_file);
    error = 1;
  }
  else if (num_insns > max_prog_insns)
  {
    fprintf (stderr, "%s: ERROR: Too many instructions in programme.\n",
             curr_file);
    error = 1;
  }
  else
  {
    if (start_pc != NULL)
    {
      warrior->init_pc = normalise (eval_expr (start_pc, 0U, &error));
      free (start_pc);
      start_pc = NULL;
    }

    warrior->num_insns = num_insns;
    warrior->insns = (cell_t *)malloc (num_insns * sizeof (cell_t));

    cell_addr_t i = 0U;
    while (insns_head != NULL)
    {
      warrior->insns[i].op_code = insns_head->op_code;
      warrior->insns[i].mode_a = insns_head->mode_a;
      warrior->insns[i].mode_b = insns_head->mode_b;
      warrior->insns[i].op_a
        = normalise (eval_expr (insns_head->op_a_expr, i, &error));
      warrior->insns[i].op_b
        = normalise (eval_expr (insns_head->op_b_expr, i, &error));

      free_expr (insns_head->op_a_expr);
      free_expr (insns_head->op_b_expr);

      tmp_insn_t *tmp_ptr = insns_head;
      insns_head = insns_head->next;
      free (tmp_ptr);

      i++;
    }
  }

  /* Clear the symbol table of all definitions. */
  clear_syms ();

  return error;
}


/* Assembles a warrior programme from the instructions given in the
   corresponding input file. WARRIOR is a pointer to the warrior
   programme being created. Returns 0 on success, 1 on failure. */
int
assemble_warrior (warrior_t *warrior)
{
  int error = 0;
  FILE *fp = NULL;

  insns_head = insns_tail = NULL;

  if (warrior->file != NULL)
  {
    fp = fopen (warrior->file, "r");
    if (fp == NULL)
    {
      fprintf (stderr, "ERROR: Could not open file \"%s\".\n", warrior->file);
      perror ("ERROR");
      error = 1;
    }
    else
    {
      curr_file = warrior->file;

      error = do_first_pass (fp, warrior);

      fclose (fp);
    }

    if (error == 0)
    {
      error = do_second_pass (warrior);
    }
  }

  return error;
}
