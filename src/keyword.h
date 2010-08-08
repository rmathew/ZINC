/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf -L ANSI-C -t -C -F ', TK_INVALID' -N find_keyword -m 10 keyword.gperf  */
/* Computed positions: -k'2-3' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "keyword.gperf"

/* Copyright (c) 2006 Ranjit Mathew. All rights reserved.
 * Use of this source code is governed by the terms of the BSD licence
 * that can be found in the LICENCE file.
 */

/*
  Keyword definitions for using with GNU gperf.
*/
#line 24 "keyword.gperf"
struct keyword
{
  const char *name;
  token_val_t val;
};
const struct keyword *find_keyword (const char *str, unsigned int len);

#define TOTAL_KEYWORDS 20
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 3
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 22
/* maximum key range = 20, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 11, 10, 23,  7,  4,
      14,  2, 23,  9, 23,  3,  0,  1,  1,  8,
       0, 23,  4, 23,  2,  9,  8, 23, 23, 23,
       9, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
      23, 23, 23, 23, 23, 23
    };
  return len + asso_values[(unsigned char)str[2]] + asso_values[(unsigned char)str[1]];
}

#ifdef __GNUC__
__inline
#endif
const struct keyword *
find_keyword (register const char *str, register unsigned int len)
{
  static const struct keyword wordlist[] =
    {
      {"", TK_INVALID}, {"", TK_INVALID}, {"", TK_INVALID},
#line 45 "keyword.gperf"
      {"SPL", TK_SPL},
#line 38 "keyword.gperf"
      {"JMP", TK_JMP},
#line 40 "keyword.gperf"
      {"JMN", TK_JMN},
#line 41 "keyword.gperf"
      {"SKL", TK_SKL},
#line 43 "keyword.gperf"
      {"SKN", TK_SKN},
#line 44 "keyword.gperf"
      {"SKG", TK_SKG},
#line 46 "keyword.gperf"
      {"ORG", TK_ORG},
#line 42 "keyword.gperf"
      {"SKE", TK_SKE},
#line 48 "keyword.gperf"
      {"VER", TK_VER},
#line 35 "keyword.gperf"
      {"MUL", TK_MUL},
#line 39 "keyword.gperf"
      {"JMZ", TK_JMZ},
#line 49 "keyword.gperf"
      {"AUT", TK_AUT},
#line 47 "keyword.gperf"
      {"NAM", TK_NAM},
#line 31 "keyword.gperf"
      {"DAT", TK_DAT},
#line 33 "keyword.gperf"
      {"ADD", TK_ADD},
#line 37 "keyword.gperf"
      {"MOD", TK_MOD},
#line 32 "keyword.gperf"
      {"MOV", TK_MOV},
#line 36 "keyword.gperf"
      {"DIV", TK_DIV},
#line 50 "keyword.gperf"
      {"DEF", TK_DEF},
#line 34 "keyword.gperf"
      {"SUB", TK_SUB}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
