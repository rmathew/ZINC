/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  The symbol table. Copied almost as-is from K&R 2nd edition.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "zinc.h"
#include "zasm.h"
#include "expr.h"
#include "sym.h"

/* The size of the hash table. This should be a prime number. */
#define HASH_SIZE 101

/* A node mapping a name to a value in the hash table. */
struct sym_node
{
  /* The mapped value (and the name). */
  sym_val_t *sym_val;

  /* The next node, if any. We resolve collisions using chaining. */
  struct sym_node *next;
};

/* The hash table. */
static struct sym_node *hash_tab[HASH_SIZE];


/* Maps the given name S to an integer between 0 and HASH_SIZE-1. */
static unsigned int
hash_code (const char *s)
{
  unsigned int hash_val;

  for (hash_val = 0U; *s != '\0'; s++)
  {
    hash_val = *s + 31 * hash_val;
  }

  return (hash_val % HASH_SIZE);
}


/* Gets the mapped value, if any, corresponding to NAME, else returns
   NULL. */
sym_val_t *
get_sym (const char *name)
{
  sym_val_t *value = NULL;

  for (struct sym_node *np = hash_tab[hash_code (name)]; np != NULL;
       np = np->next)
  {
    if (np->sym_val != NULL && strcmp (name, np->sym_val->name) == 0)
    {
      /* We found a mapping for the given name. */
      value = np->sym_val;
      break;
    }
  }

  return value;
}


/* Maps the given name NAME to the given value VALUE. If there's an existing
   value for the name, it is overridden. */
void
put_sym (const char *name, sym_val_t *value)
{
  struct sym_node *np = NULL;
  unsigned int hash_val = hash_code (name);

  for (np = hash_tab[hash_val]; np != NULL; np = np->next)
  {
    if (np->sym_val != NULL && strcmp (name, np->sym_val->name) == 0)
    {
      break;
    }
  }

  if (np != NULL)
  {
    /* We should free the existing value. */
    if (np->sym_val != NULL)
    {
      /* We retain the name. */
      value->name = np->sym_val->name;
      np->sym_val->name = NULL;

      if (np->sym_val->type == SYM_EXPR)
      {
        free_expr (np->sym_val->u.expr);
      }

      free (np->sym_val);
    }

    np->sym_val = value;
  }
  else
  {
    np = (struct sym_node *)malloc (sizeof (struct sym_node));
    np->sym_val = value;
    np->sym_val->name = (char *)malloc (strlen (name) + 1);
    strcpy (np->sym_val->name, name);
    np->next = hash_tab[hash_val];
    hash_tab[hash_val] = np;
  }
}


/* Removes all the mappings from the hash table and frees up the space
   used by the mappings. It does not destroy the hash table itself. */
void
clear_syms (void)
{
  for (int i = 0; i < HASH_SIZE; i++)
  {
    while (hash_tab[i] != NULL)
    {
      if (hash_tab[i]->sym_val != NULL)
      {
        free (hash_tab[i]->sym_val->name);

        if (hash_tab[i]->sym_val->type == SYM_EXPR)
        {
          free_expr (hash_tab[i]->sym_val->u.expr);
        }

        free (hash_tab[i]->sym_val);
      }

      struct sym_node *tmp_ptr = hash_tab[i];
      hash_tab[i] = hash_tab[i]->next;
      free (tmp_ptr);
    }
  }
}
