/* head -- output first part of file(s)
   Copyright (C) 89, 90, 91, 1995-1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Options: (see usage)
   Reads from standard input if no files are given or when a filename of
   ``-'' is encountered.
   By default, filename headers are printed only if more than one file
   is given.
   By default, prints the first 10 lines (head -n 10).

   David MacKenzie <djm@gnu.ai.mit.edu> */

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include "system.h"
#include "error.h"
#include "xstrtol.h"
#include "safe-read.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "head"

#define AUTHORS "David MacKenzie"

/* Number of lines/chars/blocks to head. */
#define DEFAULT_NUMBER 10

/* Size of atomic reads. */
#define BUFSIZE (512 * 8)

/* If nonzero, print filename headers. */
static int print_headers;

/* When to print the filename banners. */
enum header_mode
{
  multiple_files, always, never
};

/* The name this program was run with. */
char *program_name;

/* Have we ever read standard input?  */
static int have_read_stdin;

static struct option const long_options[] =
{
  {"bytes", required_argument, NULL, 'c'},
  {"lines", required_argument, NULL, 'n'},
  {"quiet", no_argument, NULL, 'q'},
  {"silent", no_argument, NULL, 'q'},
  {"verbose", no_argument, NULL, 'v'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

void
usage (int status)
{
  if (status != 0)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
	      program_name);
      printf (_("\
Print first 10 lines of each FILE to standard output.\n\
With more than one FILE, precede each with a header giving the file name.\n\
With no FILE, or when FILE is -, read standard input.\n\
\n\
  -c, --bytes=SIZE         print first SIZE bytes\n\
  -n, --lines=NUMBER       print first NUMBER lines instead of first 10\n\
  -q, --quiet, --silent    never print headers giving file names\n\
  -v, --verbose            always print headers giving file names\n\
      --help               display this help and exit\n\
      --version            output version information and exit\n\
\n\
SIZE may have a multiplier suffix: b for 512, k for 1K, m for 1 Meg.\n\
If -VALUE is used as first OPTION, read -c VALUE when one of\n\
multipliers bkm follows concatenated, else read -n VALUE.\n\
"));
      puts (_("\nReport bugs to <bug-textutils@gnu.org>."));
    }
  exit (status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void
write_header (const char *filename)
{
  static int first_file = 1;

  printf ("%s==> %s <==\n", (first_file ? "" : "\n"), filename);
  first_file = 0;
}

static int
head_bytes (const char *filename, int fd, uintmax_t bytes_to_write)
{
  char buffer[BUFSIZE];
  int bytes_read;

  /* Need BINARY I/O for the byte counts to be accurate.  */
  SET_BINARY2 (fd, fileno (stdout));

  while (bytes_to_write)
    {
      bytes_read = safe_read (fd, buffer, BUFSIZE);
      if (bytes_read < 0)
	{
	  error (0, errno, "%s", filename);
	  return 1;
	}
      if (bytes_read == 0)
	break;
      if (bytes_read > bytes_to_write)
	bytes_read = bytes_to_write;
      if (fwrite (buffer, 1, bytes_read, stdout) == 0)
	error (EXIT_FAILURE, errno, _("write error"));
      bytes_to_write -= bytes_read;
    }
  return 0;
}

static int
head_lines (const char *filename, int fd, uintmax_t lines_to_write)
{
  char buffer[BUFSIZE];

  /* Need BINARY I/O for the byte counts to be accurate.  */
  SET_BINARY2 (fd, fileno (stdout));

  while (lines_to_write)
    {
      int bytes_read = safe_read (fd, buffer, BUFSIZE);
      int bytes_to_write = 0;

      if (bytes_read < 0)
	{
	  error (0, errno, "%s", filename);
	  return 1;
	}
      if (bytes_read == 0)
	break;
      while (bytes_to_write < bytes_read)
	if (buffer[bytes_to_write++] == '\n' && --lines_to_write == 0)
	  break;
      if (fwrite (buffer, 1, bytes_to_write, stdout) == 0)
	error (EXIT_FAILURE, errno, _("write error"));
    }
  return 0;
}

static int
head (const char *filename, int fd, uintmax_t n_units, int count_lines)
{
  if (count_lines)
    return head_lines (filename, fd, n_units);
  else
    return head_bytes (filename, fd, n_units);
}

static int
head_file (const char *filename, uintmax_t n_units, int count_lines)
{
  int fd;

  if (STREQ (filename, "-"))
    {
      have_read_stdin = 1;
      filename = _("standard input");
      if (print_headers)
	write_header (filename);
      return head (filename, 0, n_units, count_lines);
    }
  else
    {
      fd = open (filename, O_RDONLY);
      if (fd >= 0)
	{
	  int errors;

	  if (print_headers)
	    write_header (filename);
	  errors = head (filename, fd, n_units, count_lines);
	  if (close (fd) == 0)
	    return errors;
	}
      error (0, errno, "%s", filename);
      return 1;
    }
}

/* Convert a string of digits, N_STRING, with a single, optional suffix
   character (b, k, or m) to an integral value.  Upon successful conversion,
   return that value.  If it cannot be converted, give a diagnostic and exit.
   COUNT_LINES indicates whether N_STRING is a number of bytes or a number
   of lines.  It is used solely to give a more specific diagnostic.  */

static uintmax_t
string_to_integer (int count_lines, const char *n_string)
{
  strtol_error s_err;
  uintmax_t n;

  s_err = xstrtoumax (n_string, NULL, 0, &n, "bkm");

  if (s_err == LONGINT_INVALID)
    {
      error (EXIT_FAILURE, 0, "%s: %s", n_string,
	     (count_lines
	      ? _("invalid number of lines")
	      : _("invalid number of bytes")));
    }

  if (s_err != LONGINT_OK)
    {
      error (EXIT_FAILURE, 0,
	     _("%s: %s is so large that it is not representable"), n_string,
	     count_lines ? _("number of lines") : _("number of bytes"));
    }

  return n;
}

int
main (int argc, char **argv)
{
  enum header_mode header_mode = multiple_files;
  int exit_status = 0;
  char *n_string;
  int c;

  /* Number of items to print. */
  uintmax_t n_units = DEFAULT_NUMBER;

  /* If nonzero, interpret the numeric argument as the number of lines.
     Otherwise, interpret it as the number of bytes.  */
  int count_lines = 1;

  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, "/usr/local/share/locale");
  textdomain (PACKAGE);

  have_read_stdin = 0;

  print_headers = 0;

  if (argc > 1 && argv[1][0] == '-' && ISDIGIT (argv[1][1]))
    {
      char *end_n_string;
      char multiplier_char = 0;

      n_string = &argv[1][1];

      /* Old option syntax; a dash, one or more digits, and one or
	 more option letters.  Move past the number. */
      for (++argv[1]; ISDIGIT (*argv[1]); ++argv[1])
	{
	  /* empty */
	}

      /* Pointer to the byte after the last digit.  */
      end_n_string = argv[1];

      /* Parse any appended option letters. */
      while (*argv[1])
	{
	  switch (*argv[1])
	    {
	    case 'c':
	      count_lines = 0;
	      multiplier_char = 0;
	      break;

	    case 'b':
	    case 'k':
	    case 'm':
	      count_lines = 0;
	      multiplier_char = *argv[1];
	      break;

	    case 'l':
	      count_lines = 1;
	      break;

	    case 'q':
	      header_mode = never;
	      break;

	    case 'v':
	      header_mode = always;
	      break;

	    default:
	      error (0, 0, _("unrecognized option `-%c'"), *argv[1]);
	      usage (1);
	    }
	  ++argv[1];
	}

      /* Append the multiplier character (if any) onto the end of
	 the digit string.  Then add NUL byte if necessary.  */
      *end_n_string = multiplier_char;
      if (multiplier_char)
	*(++end_n_string) = 0;

      n_units = string_to_integer (count_lines, n_string);

      /* Make the options we just parsed invisible to getopt. */
      argv[1] = argv[0];
      argv++;
      argc--;

      /* FIXME: allow POSIX options if there were obsolescent ones?  */

    }

  while ((c = getopt_long (argc, argv, "c:n:qv", long_options, NULL)) != -1)
    {
      switch (c)
	{
	case 0:
	  break;

	case 'c':
	  count_lines = 0;
	  n_units = string_to_integer (count_lines, optarg);
	  break;

	case 'n':
	  count_lines = 1;
	  n_units = string_to_integer (count_lines, optarg);
	  break;

	case 'q':
	  header_mode = never;
	  break;

	case 'v':
	  header_mode = always;
	  break;

	case_GETOPT_HELP_CHAR;

	case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

	default:
	  usage (1);
	}
    }

  if (header_mode == always
      || (header_mode == multiple_files && optind < argc - 1))
    print_headers = 1;

  if (optind == argc)
    exit_status |= head_file ("-", n_units, count_lines);

  for (; optind < argc; ++optind)
    exit_status |= head_file (argv[optind], n_units, count_lines);

  if (have_read_stdin && close (0) < 0)
    error (EXIT_FAILURE, errno, "-");
  if (fclose (stdout) == EOF)
    error (EXIT_FAILURE, errno, _("write error"));

  exit (exit_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
