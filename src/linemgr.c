/*
 * Global variables
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "hamt.h"

#include "globals.h"


/* Current (selected) parser */
/*@null@*/ parser *cur_parser = NULL;

/* Current (selected) object format) */
/*@null@*/ objfmt *cur_objfmt = NULL;

/* Source lines tracking */
typedef struct {
    struct line_index_mapping *vector;
    unsigned long size;
    unsigned long allocated;
} line_index_mapping_head;

typedef struct line_index_mapping {
    /* monotonically increasing line index */
    unsigned long index;

    /* related info */
    /* "original" source filename */
    /*@null@*/ /*@dependent@*/ const char *filename;
    /* "original" source base line number */
    unsigned long line;
    /* "original" source line number increment (for following lines) */
    unsigned long line_inc;
} line_index_mapping;

/* Shared storage for filenames */
static /*@only@*/ /*@null@*/ HAMT *filename_table = NULL;

/* Virtual line number.  Uniquely specifies every line read by the parser. */
unsigned long line_index = 1;
static /*@only@*/ /*@null@*/ line_index_mapping_head *line_index_map = NULL;

/* Global assembler options. */
unsigned int asm_options = 0;

/* Indentation level for assembler *_print() routines */
int indent_level = 0;

static void
filename_delete_one(/*@only@*/ void *d)
{
    xfree(d);
}

void
line_set(const char *filename, unsigned long line, unsigned long line_inc)
{
    char *copy;
    int replace = 0;
    line_index_mapping mapping;

    /* Build a mapping */

    /* Copy the filename (via shared storage) */
    copy = xstrdup(filename);
    if (!filename_table)
	filename_table = HAMT_new();
    /*@-aliasunique@*/
    mapping.filename = HAMT_insert(filename_table, copy, copy, &replace,
				    filename_delete_one);
    /*@=aliasunique@*/

    mapping.index = line_index;
    mapping.line = line;
    mapping.line_inc = line_inc;

    /* Add the mapping to the map */
    if (!line_index_map) {
	/* initialize vector */
	line_index_map = xmalloc(sizeof(line_index_mapping_head));
	line_index_map->vector = xmalloc(8*sizeof(line_index_mapping));
	line_index_map->size = 0;
	line_index_map->allocated = 8;
    }
    if (line_index_map->size >= line_index_map->allocated) {
	/* allocate another size bins when full for 2x space */
	struct line_index_mapping_s *realloc_index;
	line_index_map->vector = xrealloc(line_index_map->vector,
		2*line_index_map->allocated*sizeof(line_index_mapping));
	line_index_map->allocated *= 2;
    }
    memcpy(line_index_map->vector + line_index_map->size,
	    &mapping, sizeof(line_index_mapping));
    line_index_map->size++;

}

void
line_shutdown(void)
{
    /*line_index_mapping *mapping, *mapping2; */

    if (line_index_map) {
	/*(mapping = STAILQ_FIRST(line_index_map);
	while (mapping) {
	    mapping2 = STAILQ_NEXT(mapping, link);
	    xfree(mapping);
	    mapping = mapping2;
	}*/
	xfree(line_index_map->vector);
	xfree(line_index_map);
	line_index_map = NULL;
    }

    if (filename_table) {
	HAMT_delete(filename_table, filename_delete_one);
	filename_table = NULL;
    }
}

void
line_lookup(unsigned long lindex, const char **filename, unsigned long *line)
{
    line_index_mapping *mapping/*, *mapping2*/;
    int vindex, step;

    assert(lindex <= line_index);

    /* Binary search through map to find highest line_index <= index */
    assert(line_index_map != NULL);
    mapping = line_index_map->vector;
    vindex = 0;
    /* start step as the greatest power of 2 <= size */
    while (step*2<=line_index_map->size) step*=2;
    while (step>0) {
	if (vindex+step < line_index_map->size
		&& mapping[vindex+step].index <= lindex)
	    vindex += step;
	step /= 2;
    }
    mapping += vindex;

    assert(mapping != NULL);

    *filename = mapping->filename;
    *line = mapping->line+mapping->line_inc*(lindex-mapping->index);
}
