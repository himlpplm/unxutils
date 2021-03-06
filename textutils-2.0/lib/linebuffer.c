/* linebuffer.c -- read arbitrarily long lines
   Copyright (C) 1986, 1991, 1998, 1999 Free Software Foundation, Inc.

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

/* Written by Richard Stallman. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include "linebuffer.h"

char *xmalloc ();
char *xrealloc ();
void free ();

/* Initialize linebuffer LINEBUFFER for use. */

void
initbuffer (struct linebuffer *linebuffer)
{
  linebuffer->length = 0;
  linebuffer->size = 200;
  linebuffer->buffer = (char *) xmalloc (linebuffer->size);
}

/* Read an arbitrarily long line of text from STREAM into LINEBUFFER.
   Keep the newline; append a newline if it's the last line of a file
   that ends in a non-newline character.  Do not null terminate,
   but leave room for an extra byte after the newline.
   Return LINEBUFFER, except at end of file return 0.  */

struct linebuffer *
readline (struct linebuffer *linebuffer, FILE *stream)
{
  int c;
  char *buffer = linebuffer->buffer;
  char *p = linebuffer->buffer;
  char *end = buffer + linebuffer->size - 1; /* Sentinel. */

  if (feof (stream) || ferror (stream))
    return 0;

  do
    {
      c = getc (stream);
      if (c == EOF)
	{
	  if (p == buffer)
	    return 0;
	  if (p[-1] == '\n')
	    break;
	  c = '\n';
	}
      if (p == end)
	{
	  linebuffer->size *= 2;
	  buffer = (char *) xrealloc (buffer, linebuffer->size);
	  p = p - linebuffer->buffer + buffer;
	  linebuffer->buffer = buffer;
	  end = buffer + linebuffer->size - 1;
	}
      *p++ = c;
    }
  while (c != '\n');

  linebuffer->length = p - buffer;
  return linebuffer;
}

/* Free linebuffer LINEBUFFER and its data, all allocated with malloc. */

void
freebuffer (struct linebuffer *linebuffer)
{
  free (linebuffer->buffer);
  free (linebuffer);
}
