/* -*- coding: utf-8 -*- */
/*
 This file is part of pybliographer

 Copyright (C) 1998-1999 Frederic GOBRY <gobry@idiap.ch>
 Copyright (C) 2014-2015 Germán Poo-Caamaño <gpoo@gnome.org>

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

 $Id: bibtexmodule.c,v 1.5.2.4 2003/07/31 13:18:55 fredgo Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>
#include <structmember.h>
#include "bibtex.h"

char program_name [] = "bibtexmodule";

typedef struct {
  PyObject_HEAD
  BibtexField  *obj;
} PyBibtexField_Object;

typedef struct {
  PyObject_HEAD
  BibtexSource *db;
  PyObject *first; /* first name */
  PyObject *last;  /* last name */
  int number;
} BibtexParser;

/* Destructor of BibtexEntry */
static void destroy_field (PyBibtexField_Object * self)
{
    bibtex_field_destroy (self->obj, TRUE);

    PyObject_DEL (self);
}


static char PyBibtexField_Type__doc__[]  = "This is the type of an internal BibTeX field";

static PyTypeObject PyBibtexField_Type = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
  "BibtexField",                  /*tp_name*/
  sizeof(PyBibtexField_Object) ,  /*tp_basicsize*/
  0,                              /*tp_itemsize*/
  (destructor)destroy_field,      /*tp_dealloc*/
  (printfunc)0,                   /*tp_print*/
  (getattrfunc)0,                 /*tp_getattr*/
  (setattrfunc)0,                 /*tp_setattr*/
  0,                              /*tp_compare*/
  (reprfunc)0,                    /*tp_repr*/
  0,                              /*tp_as_number*/
  0,                              /*tp_as_sequence*/
  0,                              /*tp_as_mapping*/
  (hashfunc)0,                    /*tp_hash*/
  (ternaryfunc)0,                 /*tp_call*/
  (reprfunc)0,                    /*tp_str*/
  0,                              /*tp_getattro */
  0,                              /*tp_setattro */
  0,                              /*tp_as_buffer */
  Py_TPFLAGS_DEFAULT,             /*tp_flags */
  PyBibtexField_Type__doc__
};

static PyTypeObject BibtexParser_Type;

static void
py_message_handler (const gchar *log_domain G_GNUC_UNUSED,
		    GLogLevelFlags log_level,
		    const gchar *message,
		    gpointer user_data G_GNUC_UNUSED)
{
    PyErr_SetString (PyExc_IOError, message);
}

static char bibtex_field_get_native_doc[] =
    "field_get_native(field) -> str\n\n"
    "Get the native BibTex content of `field`.\n\n"
    "Args:\n"
    "    field (str) -- A BibTex field\n"
    "Returns:\n"
    "    A string with a native BibTex conversion of `field`.";

static PyObject *
bibtex_field_get_native (PyObject *self, PyObject *args) {
    BibtexField *field;
    PyBibtexField_Object *field_obj;
    gchar *text;
    PyObject *str;

    if (! PyArg_ParseTuple (args, "O!:field_get_native",
                            &PyBibtexField_Type, &field_obj))
        return NULL;

    field = field_obj->obj;

    if (field->structure == NULL) {
      Py_INCREF (Py_None);
      return Py_None;
    }

    text = bibtex_struct_as_bibtex (field->structure);
    str = PyUnicode_FromString (text);
    g_free (text);

    if (str == NULL)
        return NULL;

    return Py_BuildValue ("S", str);
}

static const char bibtex_field_duplicate_doc[] =
    "field_duplicate(field) -> BibtexField\n\n"
    "Makes a copy of `field`.\n\n"
    "Args:\n"
    "    field (str) -- A BibTex field\n"
    "Returns:\n"
    "    A BibtexField with a new copy of `field`.";

static PyObject *
bibtex_field_duplicate (PyObject *self, PyObject *args) {
    BibtexField *field;
    PyBibtexField_Object *field_src, *new_field;

    if (! PyArg_ParseTuple (args, "O!:field_duplicate",
                            &PyBibtexField_Type, &field_src))
        return NULL;

    field = field_src->obj;

    new_field = (PyBibtexField_Object *)
                    PyObject_NEW (PyBibtexField_Object, &PyBibtexField_Type);

    if (new_field == NULL)
        return NULL;

    new_field->obj = bibtex_struct_as_field (
                        bibtex_struct_copy (field->structure), field->type);

    return (PyObject *) new_field;
}

static char bibtex_parser_field_get_latex_doc[] =
    "field_get_latex(field_type, field) -> str\n\n"
    "Get a LaTeX representation of `field` considering its type..\n\n"
    "Args:\n"
    "    field (str) -- A BibTex field\n"
    "Returns:\n"
    "    A string with a LaTex text of `field`.";

static PyObject *
bibtex_parser_field_get_latex (BibtexParser *self, PyObject *args) {
    BibtexField *field;
    PyBibtexField_Object * field_obj;
    BibtexFieldType type;
    gchar *text;
    PyObject *str;

    if (! PyArg_ParseTuple (args, "O!i:field_get_latex",
                            &PyBibtexField_Type, &field_obj,
                            &type
                            ))
        return NULL;

    field = field_obj->obj;

    text = bibtex_struct_as_latex (field->structure, type, self->db->table);
    str = PyUnicode_FromString (text);
    g_free (text);

    if (str == NULL)
        return NULL;

    return Py_BuildValue ("S", str);
}

static const char bibtex_parser_field_set_native_doc[] =
    "field_set_native(content, field_type) -> str\n\n"
    "Set the native BibTex content of `field`.\n\n"
    "Args:\n"
    "    content (str) -- A string to convert in native format\n"
    "    field_type (int) -- Data type to convert `content`.\n"
    "Returns:\n"
    "    A BibTexField `field` or None if it cannot parse `content`.";

static PyObject *
bibtex_parser_field_set_native (BibtexParser *self, PyObject *args)
{
    PyObject *tmp;
    BibtexField *field;
    static BibtexSource *source = NULL;
    BibtexEntry *entry;
    BibtexStruct *s;
    BibtexFieldType type;
    gchar *text, *to_parse;

    if (! PyArg_ParseTuple (args, "si:field_set_native", &text, &type))
        return NULL;

    /* Create new source */
    if (source == NULL)
        source = bibtex_source_new ();

    /* parse as a string */
    to_parse = g_strdup_printf ("@preamble{%s}", text);

    if (! bibtex_source_string (source, "internal string", to_parse)) {
        PyErr_SetString (PyExc_IOError,
                         "can't create internal string for parsing");
        return NULL;
    }

    g_free (to_parse);

    entry = bibtex_source_next_entry (source, FALSE);

    if (entry == NULL) {
        return NULL;
    }

    s = bibtex_struct_copy (entry->preamble);
    bibtex_entry_destroy (entry, TRUE);

    field = bibtex_struct_as_field (s, type);

    tmp = (PyObject *) PyObject_NEW (PyBibtexField_Object, &PyBibtexField_Type);

    if (tmp == NULL)
        return NULL;

    ((PyBibtexField_Object *) tmp)->obj = field;
    return tmp;
}


static void
fill_dico (gpointer key, gpointer value, gpointer user)
{
    PyObject * dico = (PyObject *) user;
    PyObject * tmp1, * tmp2;

    tmp1 = PyUnicode_FromString ((char *) key);
    tmp2 = (PyObject *) PyObject_NEW (PyBibtexField_Object, & PyBibtexField_Type);
    /* this only happens when OOM'ing, not much to salvage except not crashing */
    if (tmp1 == NULL || tmp2 == NULL) return;

    ((PyBibtexField_Object *) tmp2)->obj = value;

    PyDict_SetItem (dico, tmp1, tmp2);

    Py_DECREF (tmp1);
    Py_DECREF (tmp2);
}

static char bibtex_parser_field_set_string_doc[] =
    "field_set_string(source, key, field) -> str\n\n"
    "Set a copy of `field` as the `field` value.\n\n"
    "Args:\n"
    "    source (BibtexSource) -- A Bibtex source object (parser).\n"
    "    key (str) -- The field key where to set `field`.\n"
    "    field (BibTexField) -- Content to set into `key`.";

static PyObject *
bibtex_parser_field_set_string (BibtexParser *self, PyObject *args)
{
    BibtexField *field;
    PyBibtexField_Object *field_obj;
    gchar *key;

    if (! PyArg_ParseTuple (args, "O!sO!:field_set_string",
                            &key,
                            &PyBibtexField_Type,
                            &field_obj
                           ))
        return NULL;

    field  = field_obj->obj;

    /* set a copy of the struct as the field value */
    bibtex_source_set_string (self->db, key,
                              bibtex_struct_copy (field->structure));

    Py_INCREF (Py_None);
    return Py_None;
}

static char bibtex_parser_field_reverse_doc[] =
    "reverse(field_type, use_braces, content) -> BibtexField\n\n";

static PyObject *
bibtex_parser_field_reverse (PyObject *self, PyObject *args)
{
    BibtexField *field;
    PyObject *tuple, *authobj, *tmp;
    BibtexFieldType type;
    BibtexAuthor *auth;
    gint length, i, brace, quote;

    if (! PyArg_ParseTuple (args, "iiO:reverse", &type, &brace, &tuple))
        return NULL;

    field = bibtex_field_new (type);

    if (field == NULL) {
        PyErr_SetString (PyExc_IOError, "can't create field");
        return NULL;
    }

    quote = 1;

    switch (field->type) {
    case BIBTEX_VERBATIM:
        quote = 0;

    case BIBTEX_OTHER:
    case BIBTEX_TITLE:
        tmp = PyObject_Str (tuple);
        if (tmp == NULL) return NULL;

        field->text = g_strdup (PyBytes_AS_STRING (tmp));
        Py_DECREF (tmp);
        break;

    case BIBTEX_DATE:
        tmp = PyObject_GetAttrString (tuple, "year");
        if (tmp == NULL) return NULL;

        if (tmp != Py_None)
            field->field.date.year  = PyLong_AsLong (tmp);
        Py_DECREF (tmp);

        tmp = PyObject_GetAttrString (tuple, "month");
        if (tmp == NULL) return NULL;

        if (tmp != Py_None)
            field->field.date.month = PyLong_AsLong (tmp);
        Py_DECREF (tmp);

        tmp = PyObject_GetAttrString (tuple, "day");
        if (tmp == NULL) return NULL;

        if (tmp != Py_None)
            field->field.date.day   = PyLong_AsLong (tmp);
        Py_DECREF (tmp);
        break;

    case BIBTEX_AUTHOR:
        length = PySequence_Length (tuple);

        if (length < 0) return NULL;

        field->field.author = bibtex_author_group_new ();

        g_array_set_size (field->field.author, length);

        for (i = 0; i < length; i++) {
            authobj = PySequence_GetItem (tuple, i);
            auth    = & g_array_index (field->field.author, BibtexAuthor, i);

            tmp = PyObject_GetAttrString (authobj, "last");
            if (tmp != Py_None) {
                auth->last = g_strdup (PyBytes_AS_STRING (tmp));
            } else {
                auth->last = NULL;
            }
            Py_DECREF (tmp);
            tmp = PyObject_GetAttrString (authobj, "first");
            if (tmp != Py_None) {
                auth->first = g_strdup (PyBytes_AS_STRING (tmp));
            } else {
                auth->first = NULL;
            }
            Py_DECREF (tmp);
            tmp = PyObject_GetAttrString (authobj, "lineage");
            if (tmp != Py_None) {
                auth->lineage = g_strdup (PyBytes_AS_STRING (tmp));
            } else {
                auth->lineage = NULL;
            }
            Py_DECREF (tmp);
            tmp = PyObject_GetAttrString (authobj, "honorific");
            if (tmp != Py_None) {
                auth->honorific = g_strdup (PyBytes_AS_STRING (tmp));
            } else {
                auth->honorific = NULL;
            }
            Py_DECREF (tmp);

            Py_DECREF (authobj);
        }
    }

    bibtex_reverse_field (field, brace, quote);

    tmp = (PyObject *) PyObject_NEW (PyBibtexField_Object, &PyBibtexField_Type);
    if (tmp == NULL) return NULL;

    ((PyBibtexField_Object *) tmp)->obj = field;
    return tmp;
}

/* Error functions */
static PyObject *
err_closed (void)
{
    PyErr_SetString (PyExc_ValueError, "I/O operation on closed file");
    return NULL;
}

/* BibtexParser implementation */

static void
bibtex_parser_dealloc (BibtexParser *self)
{
    if (self->db != NULL) {
        bibtex_source_destroy (self->db, TRUE);
    }
    Py_XDECREF (self->first);
    Py_XDECREF (self->last);

    Py_TYPE (self)->tp_free ((PyObject *) self);
}

static int
bibtex_parser_init (BibtexParser *self, PyObject *args, PyObject *kwds)
{
    PyObject *first=NULL;
    PyObject *last=NULL;
    PyObject *tmp;

    static char *kwlist[] = {"first", "last", "number", NULL};

    if (! PyArg_ParseTupleAndKeywords (args, kwds, "|OOi", kwlist,
                                       &first, &last,
                                       &self->number))
        return -1;

    if (first) {
        tmp = self->first;
        Py_INCREF (first);
        self->first = first;
        Py_XDECREF (tmp);
    }

    if (last) {
        tmp = self->last;
        Py_INCREF (last);
        self->last = last;
        Py_XDECREF (tmp);
    }

    return 0;
}

static PyObject *
bibtex_parser_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    BibtexParser *self;

    assert (type != NULL && type->tp_alloc != NULL);

    self = (BibtexParser *) type->tp_alloc (type, 0);

    if (self != NULL) {
        self->first = PyUnicode_FromString ("");
        if (self->first == NULL) {
            Py_DECREF (self);
            return NULL;
        }
        self->last = PyUnicode_FromString ("");
        if (self->last == NULL) {
            Py_DECREF (self);
            return NULL;
        }
        self->db = NULL;
        self->number = 0;
    }

    return (PyObject *) self;
}

static PyObject *
bibtex_parser_self (BibtexParser *self)
{
    if (self->db == NULL)
        return err_closed ();

    Py_INCREF (self);
    return (PyObject *) self;
}

/* Used by BibtexParser_iternext
 *
 * Returns:
 *     A tuple ('entry', key, field_type, offset, line, object)
 */
static PyObject *
get_next_entry (BibtexParser *self)
{
    BibtexEntry *entry;
    PyObject *dico, *result, *name;

    /* Get the next entry unfiltered */
    entry = bibtex_source_next_entry_unfiltered (self->db);

    if (entry == NULL) {
        if (self->db->eof) {
            bibtex_source_rewind (self->db);
        }
        return NULL;
    }

    /* Retour de la fonction */
    if (! entry->name) {
        if (entry->textual_preamble) {
            result = Py_BuildValue ("ss", entry->type,
                                    entry->textual_preamble);
        } else {
            /* this must be a string then... */
            result = Py_BuildValue ("(s)", entry->type);
        }
    } else {
        dico = PyDict_New ();

        g_hash_table_foreach (entry->table, fill_dico, dico);

        if (entry->name) {
            name = PyUnicode_FromString (entry->name);
        } else {
            name = Py_None;
            Py_INCREF (name);
        }

        result = Py_BuildValue ("(s(NsiiO))", "entry", name,
                                entry->type, entry->offset,
                                entry->start_line, dico);

        Py_DECREF (dico);
    }

    bibtex_entry_destroy (entry, FALSE);

    return result;
}

static PyObject *
bibtex_parser_iternext (BibtexParser *self)
{
    PyObject *entry;

    if (self->db == NULL)
        return err_closed ();

    entry = get_next_entry (self);

    return entry;
}

static const char bibtex_parser_field_expand_doc[] =
    "field_expand(source, field, field_type) -> Tuple\n\n"
    "Expand a Bibtex field given its type.  Currently it can split\n"
    "`author` names, set a date tuple for `year`, and expand the `title`\n"
    "In all these cases, any LaTeX command will be converted to text.\n\n"
    "Args:\n"
    "    source (BibtexSource) -- A Bibtex source object (parser).\n"
    "    field (str) -- A single raw BibTex field or record.\n"
    "    field_type (int) -- Data type of field.\n"
    "Returns:\n"
    "    A tuple of three or more values depending of `field_type`.\n"
    "    The first two value indicates the `type` parsed, and whether\n"
    "    there was loss of information during the conversion (`loss`).\n"
    "      Date -> (type, loss, year, month, day)\n"
    "              In case of no data, the values will be set to 0.\n"
    "      Author -> (type, loss, [(honorific, first, last, lineage),...])\n"
    "              In case of no data, the values will be set to None.\n"
    "      Title -> (type, loss, content)\n"
    "      Verbatim -> (type, loss, raw_content).\n"
    "      Other -> (type, loss, content)";

static PyObject *
bibtex_parser_field_expand (BibtexParser *self, PyObject *args)
{
    PyObject *liste, *tmp, *auth[4];
    BibtexFieldType type;
    BibtexField *field;
    PyBibtexField_Object *field_obj;
    BibtexAuthor *author;

    int i, j;

    if (! PyArg_ParseTuple (args, "O!i:field_expand",
                           &PyBibtexField_Type, &field_obj,
                           &type))
        return NULL;

    field = field_obj->obj;

    if (! field->converted) {
        if (type != -1) {
            field->type = type;
        }

        bibtex_field_parse (field, self->db->table);
    }

    switch (field->type) {
    case BIBTEX_TITLE:
    case BIBTEX_OTHER:
    case BIBTEX_VERBATIM:
        tmp = Py_BuildValue ("iis", field->type, field->loss,
                             field->text);
        break;
    case BIBTEX_DATE:
        tmp = Py_BuildValue ("iisiii",
                             field->type,
                             field->loss,
                             field->text,
                             field->field.date.year,
                             field->field.date.month,
                             field->field.date.day);
        break;

    case BIBTEX_AUTHOR:
        liste = PyList_New (field->field.author->len);

        for (i = 0; i < field->field.author->len; i++) {
            author = &g_array_index (field->field.author,
                                     BibtexAuthor, i);
            if (author->honorific) {
                auth[0] = PyUnicode_FromString (author->honorific);
            } else {
                auth[0] = Py_None;
                Py_INCREF (Py_None);
            }

            if (author->first) {
                auth [1] = PyUnicode_FromString (author->first);
            } else {
                auth[1] = Py_None;
                Py_INCREF (Py_None);
            }

            if (author->last) {
                auth[2] = PyUnicode_FromString (author->last);
            } else {
                auth[2] = Py_None;
                Py_INCREF (Py_None);
            }

            if (author->lineage) {
                auth[3] = PyUnicode_FromString (author->lineage);
            } else {
                auth[3] = Py_None;
                Py_INCREF (Py_None);
            }

            PyList_SetItem (liste, i, Py_BuildValue ("OOOO", auth[0],
                                                     auth[1], auth[2],
                                                     auth[3]));

            for (j = 0; j < 4; j ++) {
                Py_DECREF (auth[j]);
            }
        }

        tmp = Py_BuildValue ("iisO", field->type, field->loss,
                             field->text, liste);
        Py_DECREF (liste);
        break;

    default:
        tmp = Py_None;
        Py_INCREF (Py_None);
    }

    return tmp;
}

static const char bibtex_parser_name_doc[] =
    "Return the name, combining the first and last name";

static PyObject *
bibtex_parser_name (BibtexParser *self)
{
    static PyObject *format = NULL;
    PyObject *args, *result;

    if (format == NULL) {
        format = PyUnicode_FromString ("%s %s");
        if (format == NULL)
            return NULL;
    }

    if (self->first == NULL) {
        PyErr_SetString (PyExc_AttributeError, "first");
        return NULL;
    }

    if (self->last == NULL) {
        PyErr_SetString (PyExc_AttributeError, "last");
        return NULL;
    }

    args = Py_BuildValue ("OO", self->first, self->last);
    if (args == NULL)
        return NULL;

    result = PyUnicode_Format (format, args);
    Py_DECREF (args);

    return result;
}

static const char bibtex_parser_open_file_doc[] =
    "new_from_file(filename, strictness) -> BibtexParser object\n\n"
    "Create an object for the specified filename to parse from.\n\n"
    "Args:\n"
    "    filename (str) -- The BibTex file name\n"
    "    stricness (boolean) -- Set the parser strict or lousy\n"
    "Returns:\n"
    "    A BibtexParser object to start parsing from";

static PyObject *
bibtex_parser_open_file (PyObject *o, PyObject *args)
{
    BibtexSource *db;
    BibtexParser *self;
    gboolean strictness;
    char *name;

    if (! PyArg_ParseTuple (args, "si:new_from_file", &name, &strictness))
        return NULL;

    self = (BibtexParser *) PyObject_NEW (BibtexParser, &BibtexParser_Type);

    if (self == NULL)
        return NULL;

    self->first = PyUnicode_FromString ("");
    if (self->first == NULL) {
        Py_DECREF (self);
        return NULL;
    }
    self->last = PyUnicode_FromString ("");
    if (self->last == NULL) {
        Py_DECREF (self);
        return NULL;
    }
    self->number = 0;

    db = bibtex_source_new ();
    db->strict = strictness;

    if (! bibtex_source_file (db, name)) {
        Py_DECREF (self);
        bibtex_source_destroy (db, TRUE);
        return NULL;
    }

    self->db = db;
    bibtex_source_rewind (self->db);

    return (PyObject *) self;
}

static char bibtex_parser_new_from_string_doc[] =
    "new_from_string(name, string, strictness) -> BibtexSource object\n\n"
    "Create an object for the specified filename to parse from.\n\n"
    "Args:\n"
    "    name (str) -- A BibTex tag name.\n"
    "    string (str) -- String to parse.\n"
    "    stricness (boolean) -- Set the parser strict or lousy.\n"
    "Returns:\n"
    "    A BibtexSource object to start parsing from.";

static PyObject *
bibtex_parser_new_from_string (PyObject *o, PyObject *args)
{
    BibtexSource *db;
    BibtexParser *self;
    gboolean strictness;
    char *name, *string;

    if (! PyArg_ParseTuple (args, "ssi:new_from_string", &name,
                           &string, &strictness))
	return NULL;

    self = (BibtexParser *) PyObject_NEW (BibtexParser, &BibtexParser_Type);

    if (self == NULL)
        return NULL;

    self->first = PyUnicode_FromString ("");
    if (self->first == NULL) {
        Py_DECREF (self);
        return NULL;
    }
    self->last = PyUnicode_FromString ("");
    if (self->last == NULL) {
        Py_DECREF (self);
        return NULL;
    }
    self->number = 0;

    db = bibtex_source_new ();
    db->strict = strictness;

    if (! bibtex_source_string (db, name, string)) {
        Py_DECREF (self);
        bibtex_source_destroy (db, TRUE);
        return NULL;
    }

    self->db = db;
    bibtex_source_rewind (self->db);

    return (PyObject *) self;
}

/* BibtexParser attributes */
static PyMemberDef BibtexParser_members[] = {
  {"db",    T_OBJECT_EX, offsetof (BibtexParser, db), 0,
   "source object"},
  {"first", T_OBJECT_EX, offsetof (BibtexParser, first), 0,
   "first name"},
  {"last",  T_OBJECT_EX, offsetof (BibtexParser, last), 0,
   "last name"},
  {"number", T_INT,      offsetof (BibtexParser, number), 0,
   "noddy number"},
  {NULL}  /* Sentinel */
};

/* BibtexParser methods' definitions */
static PyMethodDef BibtexParser_methods[] = {
    {"name", (PyCFunction) bibtex_parser_name, METH_NOARGS,
     bibtex_parser_name_doc },
    {"field_expand", (PyCFunction) bibtex_parser_field_expand, METH_VARARGS,
     bibtex_parser_field_expand_doc },
    { "field_get_native", (PyCFunction) bibtex_field_get_native, METH_VARARGS,
     bibtex_field_get_native_doc },
    { "field_set_native", (PyCFunction) bibtex_parser_field_set_native,
     METH_VARARGS, bibtex_parser_field_set_native_doc },
    { "field_duplicate", (PyCFunction) bibtex_field_duplicate, METH_VARARGS,
     bibtex_field_duplicate_doc },
    { "field_get_latex", (PyCFunction) bibtex_parser_field_get_latex, METH_VARARGS,
     bibtex_parser_field_get_latex_doc },
    { "field_set_string", (PyCFunction) bibtex_parser_field_set_string, METH_VARARGS,
     bibtex_parser_field_set_string_doc },
    { "field_reverse", (PyCFunction) bibtex_parser_field_reverse,
     METH_VARARGS, bibtex_parser_field_reverse_doc },
    {NULL}  /* Sentinel */
};

/* BibtexParser definition */

static const char BibtexParser_doc[] =
  "This is the type of a BibTeX Parser";

static PyTypeObject BibtexParser_Type = {
  PyVarObject_HEAD_INIT (NULL, 0)
  "BibtexParser",                       /*tp_name*/
  sizeof (BibtexParser),                /*tp_basicsize*/
  0,                                    /*tp_itemsize*/
  (destructor) bibtex_parser_dealloc,   /*tp_dealloc*/
  0,                                    /*tp_print*/
  0,                                    /*tp_getattr*/
  0,                                    /*tp_setattr*/
  0,                                    /*tp_compare*/
  0,                                    /*tp_repr*/
  0,                                    /*tp_as_number*/
  0,                                    /*tp_as_sequence*/
  0,                                    /*tp_as_mapping*/
  0,                                    /*tp_hash*/
  0,                                    /*tp_call*/
  0,                                    /*tp_str*/
  0,                                    /*tp_getattro */
  0,                                    /*tp_setattro */
  0,                                    /*tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags */
  BibtexParser_doc,                     /* tp_doc */
  0,                                    /* tp_traverse */
  0,                                    /* tp_clear */
  0,                                    /* tp_richcompare */
  0,                                    /* tp_weaklistoffset */
  (getiterfunc) bibtex_parser_self,     /* tp_iter */
  (iternextfunc) bibtex_parser_iternext, /* tp_iternext */
  BibtexParser_methods,                 /* tp_methods */
  BibtexParser_members,                 /* tp_members */
  0,                                    /* tp_getset */
  0,                                    /* tp_base */
  0,                                    /* tp_dict */
  0,                                    /* tp_descr_get */
  0,                                    /* tp_descr_set */
  0,                                    /* tp_dictoffset */
  (initproc) bibtex_parser_init,        /* tp_init */
  0,                                    /* tp_alloc */
  bibtex_parser_new,                     /* tp_new */
};


/* Main program */

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*) PyModule_GetState (m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

static PyMethodDef bibtex_methods [] = {
    { "new_from_file", bibtex_parser_open_file, METH_VARARGS,
	               bibtex_parser_open_file_doc },
    { "new_from_string", bibtex_parser_new_from_string, METH_VARARGS,
	               bibtex_parser_new_from_string_doc },
    {NULL}  /* Sentinel */
};

static char bibtex_doc[] =
    "A BibTeX parser\n\n"
    "This module provides the components needed to parse a BibTex file\n";

#if PY_MAJOR_VERSION >= 3

static int bibtex_traverse (PyObject *m, visitproc visit, void *arg) {
    Py_VISIT (GETSTATE(m)->error);
    return 0;
}

static int bibtex_clear (PyObject *m) {
    Py_CLEAR (GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "bibtex",
        bibtex_doc,
        sizeof (struct module_state),
        bibtex_methods,
        NULL,
        bibtex_traverse,
        bibtex_clear,
        NULL
};

#define INITERROR return NULL

PyObject *
PyInit_bibtex (void)

#else
#define INITERROR return

void
initbibtex (void)
#endif
{
    PyObject *module;

    // PyBibtexSource_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready (&BibtexParser_Type) < 0)
        INITERROR;

    //PyBibtexField_Type.ob_type =  &PyType_Type;

    bibtex_set_default_handler ();

    g_log_set_handler (G_LOG_DOMAIN, BIB_LEVEL_ERROR,
                       py_message_handler, NULL);
    g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL);

#if PY_MAJOR_VERSION >= 3
    module = PyModule_Create(&moduledef);
#else
    module = Py_InitModule3 ("bibtex", bibtex_methods, bibtex_doc);
#endif

    if (module == NULL)
        INITERROR;
    struct module_state *st = GETSTATE(module);

    st->error = PyErr_NewException("bibtex.Error", NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }

    Py_INCREF (&BibtexParser_Type);
    PyModule_AddObject (module, "BibtexParser",
                        (PyObject *) &BibtexParser_Type);

#if PY_MAJOR_VERSION >= 3
    return module;
#endif

}

/* vim: set ts=4 sw=4 st=4 expandtab */
