/*
 This file is part of pybliographer
 
 Copyright (C) 1998-1999 Frederic GOBRY
 Email : gobry@idiap.ch
 	   
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2 
 of the License, or (at your option) any later version.
   
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details. 
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 $Id: source.c,v 1.1.2.1 2003/09/01 19:54:46 fredgo Exp $
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include "bibtex.h"

BibtexSource * 
bibtex_source_new (void) {
    BibtexSource * new;

    new = g_new (BibtexSource, 1);

    new->name  = NULL;
    new->type  = BIBTEX_SOURCE_NONE;
    new->table = g_hash_table_new (g_str_hash, g_str_equal);
    new->debug = FALSE;
    new->buffer = NULL;
    new->strict = TRUE;

    return new;
}

static void
reset_source (BibtexSource * source) {

    bibtex_analyzer_finish (source);

    if (source->name) {
	g_free (source->name);
    }
    
    switch (source->type) {
    case BIBTEX_SOURCE_NONE:
	break;

    case BIBTEX_SOURCE_FILE:
	fclose (source->source.file);
	break;

    case BIBTEX_SOURCE_STRING:
	g_free (source->source.string);
	break;

    default:
	g_assert_not_reached ();
    }

    source->offset = 0;
    source->line   = 1;
    source->eof    = FALSE;
    source->error  = FALSE;
}

static void
freedata (gpointer key, 
	   gpointer value, 
	   gpointer user) {

    g_free (key);

    if ((gboolean) GPOINTER_TO_INT(user)) {
	bibtex_struct_destroy ((BibtexStruct *) value, TRUE);
    }
}

BibtexStruct * 
bibtex_source_get_string (BibtexSource * source,
			  gchar * key) {
    g_return_val_if_fail (source != NULL, NULL);
    g_return_val_if_fail (key != NULL, NULL);

    return g_hash_table_lookup (source->table, key);
}

void
bibtex_source_set_string (BibtexSource * source,
			  gchar * key,
			  BibtexStruct * value) {
    gchar * inskey = key;
    BibtexStruct * oldstruct = NULL;

    g_return_if_fail (source != NULL);
    g_return_if_fail (key != NULL);

    if ((oldstruct = g_hash_table_lookup (source->table, inskey)) == NULL) {
	inskey = g_ascii_strdown(key, -1);
    }
    else {
	bibtex_struct_destroy(oldstruct, TRUE);
    }

    g_hash_table_insert (source->table, inskey, value);
}

void           
bibtex_source_destroy (BibtexSource * source,
		       gboolean free_data) {
    g_return_if_fail (source != NULL);

    g_hash_table_foreach (source->table, freedata, GINT_TO_POINTER(free_data));
    g_hash_table_destroy (source->table);

    reset_source (source);

    g_free (source);
}


gboolean
bibtex_source_file (BibtexSource * source, 
		    gchar * filename) {
    FILE * fh = NULL;

    g_return_val_if_fail (source != NULL, FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    
    fh = fopen (filename, "r");
    if (fh == NULL) {
	bibtex_error ("can't open file `%s': %s",
		      filename,
		      g_strerror (errno));
	return FALSE;
    }

    reset_source (source);

    source->type = BIBTEX_SOURCE_FILE;
    source->name = g_strdup (filename);
    source->source.file = fh;
    
    bibtex_analyzer_initialize (source);

    return TRUE;
}


gboolean
bibtex_source_string (BibtexSource * source, 
		      gchar * name,
		      gchar * string) {
    g_return_val_if_fail (source != NULL, FALSE);
    g_return_val_if_fail (string != NULL, FALSE);

    reset_source (source);

    source->type = BIBTEX_SOURCE_STRING;

    if (name) {
	source->name = g_strdup (name);
    }
    else {
	source->name = g_strdup ("<string>");
    }

    source->source.string = g_strdup (string);
    
    bibtex_analyzer_initialize (source);

    return TRUE;
}

void 
bibtex_source_rewind (BibtexSource * file) {

    bibtex_source_set_offset (file, 0);
}

gint 
bibtex_source_get_offset (BibtexSource * file) {
    g_return_val_if_fail (file != NULL, -1);

    return file->offset;
}
    
void
bibtex_source_set_offset (BibtexSource * file, 
			  gint offset) {
    g_return_if_fail (file != NULL);

    bibtex_analyzer_finish (file);

    switch (file->type) {
    case BIBTEX_SOURCE_FILE:
	if (fseek (file->source.file, offset, SEEK_SET) == -1) {
	    bibtex_error ("%s: can't jump to offset %d: %s", 
			  file->name,
			  offset, g_strerror (errno));
	    file->error = TRUE;
	    return;
	}
	break;

    case BIBTEX_SOURCE_STRING:
	break;

    case BIBTEX_SOURCE_NONE:
	g_warning ("no source to set offset");
	break;
    }

    file->offset = offset;
    file->eof    = file->error = FALSE;

    bibtex_analyzer_initialize (file);
}

static void
add_to_dico (gpointer key, gpointer value, gpointer user) {
    gchar * val = (gchar *) key;
    BibtexField * field;
    BibtexStruct * structure;

    if ((structure = g_hash_table_lookup ((GHashTable *) user, val)) == NULL) {
	val = g_strdup ((char *) key);
    }
    else {
	bibtex_struct_destroy (structure, TRUE);
    }

    field = (BibtexField *) value;

    gchar* c = val;
    for (; *c != '\0'; *c = g_ascii_tolower(*c), c++);

    g_hash_table_insert ((GHashTable *) user, val, field->structure);
}

BibtexEntry *
bibtex_source_next_entry (BibtexSource * file,
			  gboolean filter) {
    BibtexEntry * ent;

    int offset;

    g_return_val_if_fail (file != NULL, NULL);

    if (file->eof) return NULL;

    offset = file->offset;
    file->error = FALSE;

    do {
	ent = bibtex_analyzer_parse (file);

	if (ent) {
	    /* Incrementer les numeros de ligne */
	    file->line += ent->length;
	    
	    ent->offset = offset;
	    ent->length = file->offset - offset;
	    
	    /* Rajouter les definitions au dictionnaire, si necessaire */
	    if (ent->type) {
		if (g_ascii_strcasecmp (ent->type, "string") == 0) {

		    g_hash_table_foreach (ent->table, add_to_dico, 
					  file->table);
		    
		    if (filter) {
			/* Return nothing, we store it as string database */
			bibtex_entry_destroy (ent, FALSE);
			ent = NULL;
		    }
		}
		else {
		    do {
			if (g_ascii_strcasecmp (ent->type, "comment") == 0) {
			    bibtex_entry_destroy (ent, TRUE);
			    ent = NULL;
			    
			    break;
			}

			if (g_ascii_strcasecmp (ent->type, "preamble") == 0) {
			    if (filter) {
				bibtex_warning ("%s:%d: skipping preamble",
						file->name, file->line);
				
				bibtex_entry_destroy (ent, TRUE);
				ent = NULL;
			    }
			    else {
				ent->textual_preamble =
				    bibtex_struct_as_bibtex (ent->preamble);
			    }
			    break;
			}

			/* normal case; convert preamble into entry name */
			if (ent->preamble) {
			    switch (ent->preamble->type) {
			    case BIBTEX_STRUCT_REF:
				/* alphanumeric identifiers */
				ent->name = 
				    g_strdup (ent->preamble->value.ref);
				break;
			    case BIBTEX_STRUCT_TEXT:
				/* numeric identifiers */
				ent->name = 
				    g_strdup (ent->preamble->value.text);
				break;

			    default:
				if (file->strict) {
				    bibtex_error ("%s:%d: entry has a weird name", 
						  file->name, file->line);
				    bibtex_entry_destroy (ent, TRUE);
				    file->error = TRUE;
				    
				    return NULL;
				}
				else {
				    bibtex_warning ("%s:%d: entry has a weird name", 
						    file->name, file->line);
				    bibtex_struct_destroy (ent->preamble, TRUE);
				    ent->preamble = NULL;
				    ent->name = NULL;
				}
				break;
			    }
			}
			else {
			    if (file->strict) {
				bibtex_error ("%s:%d: entry has no identifier", 
					      file->name,
					      file->line);
				
				bibtex_entry_destroy (ent, TRUE);
				file->error = TRUE;
				
				return NULL;
			    }
			    else {
				bibtex_warning ("%s:%d: entry has no identifier", 
						file->name,
						file->line);
			    }
			}
		    } while (0);
		}
	    }
	}
	else {
	    /* No ent, there has been a problem */
	    return NULL;
	}
    } 
    while (!ent);

    return ent;
}


