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
 
 $Id: bibtex.c,v 1.1.2.2 2003/09/02 14:35:33 fredgo Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>

#include "bibtex.h"

void bibtex_message_handler (const gchar *log_domain G_GNUC_UNUSED,
			     GLogLevelFlags log_level,
			     const gchar *message,
			     gpointer user_data G_GNUC_UNUSED)
{
    const gchar * name = g_get_prgname ();

    if (name) {
	fprintf (stderr, "%s: ", name);
    }

    switch ((int)log_level) {

    case BIB_LEVEL_ERROR:
	fprintf (stderr, "error: %s\n", message);
	break;
    case BIB_LEVEL_WARNING:
	fprintf (stderr, "warning: %s\n", message);
	break;
    case BIB_LEVEL_MESSAGE:
	printf ("%s\n", message);
	break;
	
    default:
	fprintf (stderr, "<unknown level %d>: %s\n", log_level, message);
	break;
    }
}

void 
bibtex_set_default_handler (void) {
    g_log_set_handler (G_LOG_DOMAIN, BIB_LEVEL_ERROR,   
		       bibtex_message_handler, NULL);
    g_log_set_handler (G_LOG_DOMAIN, BIB_LEVEL_WARNING, 
		       bibtex_message_handler, NULL);
    g_log_set_handler (G_LOG_DOMAIN, BIB_LEVEL_MESSAGE, 
		       bibtex_message_handler, NULL);
}
