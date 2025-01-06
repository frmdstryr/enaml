/*-----------------------------------------------------------------------------
 * | Copyright (c) 2025, Nucleic Development Team.
 * |
 * | Distributed under the terms of the Modified BSD License.
 * |
 * | The full license is in the file LICENSE, distributed with this software.
 * |----------------------------------------------------------------------------*/
#include <iostream>
#include <sstream>
#include <cppy/cppy.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeprecated-writable-strings"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif


namespace enaml
{

// POD struct - all member fields are considered private
struct StandardTracer
{
    PyObject_HEAD
    PyObject* owner;
    PyObject* name;
    PyObject* key;
    PyObject* items;

    static PyType_Spec TypeObject_Spec;
    static PyTypeObject* TypeObject;

    static bool Ready();
    static bool TypeCheck( PyObject* ob );

};


namespace
{


PyObject*
StandardTracer_new( PyTypeObject* type, PyObject* args, PyObject* kwargs )
{
    PyObject* owner;
    PyObject* name;
    static char* kwlist[] = { "owner", "name", 0 };
    if( !PyArg_ParseTupleAndKeywords( args, kwargs, "OU", kwlist, &owner, &name ) )
        return 0;

    cppy::ptr ptr( PyType_GenericNew( type, args, kwargs ) );
    if( !ptr )
        return 0;

    StandardTracer* self = reinterpret_cast<StandardTracer*>( ptr.get() );
    self->owner =  cppy::incref( owner );
    self->name =  cppy::incref( name );
    self->items = PySet_New( 0 );
    self->key = PyUnicode_FromFormat("_[%U|trace]", name);
    if ( !self->items || !self->key )
        return 0;
    return ptr.release();
}


void
StandardTracer_clear( StandardTracer* self )
{
    Py_CLEAR( self->owner );
    Py_CLEAR( self->name );
    Py_CLEAR( self->key );
    Py_CLEAR( self->items );
}


int
StandardTracer_traverse( StandardTracer* self, visitproc visit, void* arg )
{
    Py_VISIT( self->owner );
    Py_VISIT( self->name );
    Py_VISIT( self->key );
    Py_VISIT( self->items );
    #if PY_VERSION_HEX >= 0x03090000
    // This was not needed before Python 3.9 (Python issue 35810 and 40217)
    Py_VISIT(Py_TYPE(self));
    #endif
    return 0;
}


void
StandardTracer_dealloc( StandardTracer* self )
{
    PyObject_GC_UnTrack( self );
    StandardTracer_clear( self );
    Py_TYPE(self)->tp_free( reinterpret_cast<PyObject*>( self ) );
}



static bool is_alias( PyObject* obj )
{
    cppy::ptr alias_module( PyImport_ImportModule("enaml.core.alias") );
    if ( !alias_module )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import enaml.core.alias" );
        return 0;
    }
    cppy::ptr Alias( alias_module.getattr("Alias") );
    if ( !Alias )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import enaml.core.alias.Alias" );
        return 0;
    }

    const int r =  PyObject_IsInstance( obj, Alias.get() );
    if (r == 1)
        return true;
    if (r == 0)
        return false;
    PyErr_Clear();
    return 0;
}

static bool is_atom_instance( PyObject* obj )
{
    cppy::ptr atom_api( PyImport_ImportModule("atom.api") );
    if ( !atom_api )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import atom.api" );
        return 0;
    }
    cppy::ptr Atom( atom_api.getattr("Atom") );
    if ( !Atom )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import atom.api.Atom" );
        return 0;
    }

    const int r =  PyObject_IsInstance( obj, Atom.get() );
    if (r == 1)
        return 1;
    if (r == 0)
        return 0;
    PyErr_Clear();
    return 0;
}


static bool is_getattr( PyObject* obj )
{
    cppy::ptr builtins( PyImport_ImportModule("builtins") );
    if ( !builtins )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import builtins" );
        return 0;
    }
    cppy::ptr getattr( builtins.getattr("getattr") );
    if ( !getattr )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import builtins.getattr" );
        return 0;
    }
    return obj == getattr;
}



/*
* Add the atom object and name pair to the traced items.
* if obj.get_member(name) is not None:
*     self.items.add((obj, name))
* else:
*    alias = getattr(type(obj), name, None)
*    if isinstance(alias, Alias):
*        alias_obj, alias_attr = alias.resolve(obj)
*        if alias_attr:
*            self.trace_atom(alias_obj, alias_attr)
*/
PyObject*
_StandardTracer_trace_atom_internal( StandardTracer* self, PyObject* obj, PyObject* name )
{
    cppy::ptr objptr( cppy::incref(obj) );
    cppy::ptr nameptr( cppy::incref(name) );
    cppy::ptr get_member( objptr.getattr("get_member") );
    if ( !get_member )
        return 0;
    cppy::ptr member( PyObject_CallOneArg( get_member.get(), nameptr.get() ) );
    if ( !member )
        return 0;
    if ( !member.is_none() )
    {
        cppy::ptr item( PyTuple_New (2) );
        if( !item )
            return 0;
        PyTuple_SET_ITEM( item.get(), 0, cppy::incref( objptr.get() ) );
        PyTuple_SET_ITEM( item.get(), 1, cppy::incref( nameptr.get() ) );
        if ( PySet_Add( self->items, item.get()) )
            return 0;
    }
    else {
        cppy::ptr objtype( PyObject_Type( obj ) );
        if ( !objtype )
            return 0;
        cppy::ptr aliasptr( objtype.getattr( nameptr.get() ) );
        if ( !aliasptr )
            PyErr_Clear(); // getattr(type(obj), name) is None
        else if ( is_alias( aliasptr.get() ) ) {
            cppy::ptr resolve = aliasptr.getattr("resolve");
            cppy::ptr alias_result( PyObject_CallOneArg( resolve.get(), objptr.get() ) );
            if ( !alias_result
                || !PyTuple_Check(alias_result.get())
                || PyTuple_Size(alias_result.get()) != 2 )
                return 0;
            cppy::ptr alias_obj( cppy::incref( PyTuple_GET_ITEM(alias_result.get(), 0)) );
            cppy::ptr alias_attr( cppy::incref( PyTuple_GET_ITEM(alias_result.get(), 1)) );
            if ( !alias_attr.is_none() )
                return _StandardTracer_trace_atom_internal(self, alias_obj.get(), alias_attr.get());
        }
    }
    Py_RETURN_NONE;
}

PyObject*
StandardTracer_trace_atom( StandardTracer* self, PyObject* args, PyObject* kwargs )
{
    PyObject* obj;
    PyObject* name;
    static char* kwlist[] = { "obj", "name", 0 };
    if( !PyArg_ParseTupleAndKeywords( args, kwargs, "OU", kwlist, &obj, &name ) )
        return 0;
    return _StandardTracer_trace_atom_internal(self, obj, name);
}

/*
 * storage = owner._d_storage
 *
 * # invalidate the old observer so that* it can be collected
 * old_observer = storage.get(key)
 * if old_observer is not None:
 *    old_observer.ref = None
 *
 *    # create a new observer and subscribe it to the dependencies
 *    if self.items:
 *        observer = SubscriptionObserver(owner, name)
 *        storage[key] = observer
 *        for obj, d_name in self.items:
 *            obj.observe(d_name, observer)
*/
PyObject*
StandardTracer_finalize( StandardTracer* self )
{
    cppy::ptr owner( cppy::incref(self->owner) );
    cppy::ptr key( cppy::incref(self->key) );
    cppy::ptr storage( owner.getattr("_d_storage") );
    if ( !storage )
        return 0;

    // invalidate the old observer so that it can be collected
    cppy::ptr old_observer( PyObject_GetItem( storage.get(), key.get() ) );
    if ( !old_observer )
        PyErr_Clear();
    else
        old_observer.setattr("ref", cppy::incref(Py_None));

    cppy::ptr items( cppy::incref(self->items) );
    if ( items.is_truthy() )
    {
        cppy::ptr subscription_observer( PyImport_ImportModule("enaml.core.subscription_observer") );
        if ( !subscription_observer )
        {
            PyErr_SetString( PyExc_ImportError, "Could not import enaml.core.subscription_observer" );
            return 0;
        }
        cppy::ptr SubscriptionObserver( subscription_observer.getattr("SubscriptionObserver") );
        if ( !SubscriptionObserver )
        {
            PyErr_SetString( PyExc_ImportError, "Could not import enaml.core.subscription_observer.SubscriptionObserver" );
            return 0;
        }

        cppy::ptr observer_args( PyTuple_New (2) );
        if( !observer_args )
            return 0;
        PyTuple_SET_ITEM( observer_args.get(), 0, cppy::incref( owner.get() ) );
        PyTuple_SET_ITEM( observer_args.get(), 1, cppy::incref( self->name ) );

        cppy::ptr observer( SubscriptionObserver.call( observer_args.get() ) );
        if ( !observer )
            return 0;

        PyObject_SetItem( storage.get(), key.get(), cppy::incref( observer.get() ) );

        cppy::ptr item;
        cppy::ptr iter( items.iter() );

        while ( (item = iter.next())  )
        {
            if ( !PyTuple_Check(item.get()) || PyTuple_Size(item.get()) != 2 )
                return 0;
            cppy::ptr obj( cppy::incref( PyTuple_GET_ITEM(item.get(), 0) ) );
            cppy::ptr d_name( cppy::incref( PyTuple_GET_ITEM(item.get(), 1) ) );
            cppy::ptr args( PyTuple_New (2) );
            if( !args )
                return 0;
            PyTuple_SET_ITEM( args.get(), 0, cppy::incref( d_name.get() ) );
            PyTuple_SET_ITEM( args.get(), 1, cppy::incref( observer.get() ) );
            cppy::ptr observe( obj.getattr("observe") );
            if( !observe )
                return 0;
            cppy::ptr result( observe.call( args ) );
            if ( !result )
                return 0;
        }
    }
    Py_RETURN_NONE;
}


PyObject*
StandardTracer_dyanmic_load( StandardTracer* self, PyObject* args, PyObject* kwargs  )
{
    PyObject* obj;
    PyObject* attr;
    PyObject* value;
    static char* kwlist[] = { "obj", "attr", "value", 0 };
    if( !PyArg_ParseTupleAndKeywords( args, kwargs, "OUO", kwlist, &obj, &attr, &value ) )
        return 0;
    cppy::ptr objptr( cppy::incref(obj) );
    cppy::ptr attrptr( cppy::incref(attr) );
    if ( is_atom_instance( objptr.get() ) )
        return _StandardTracer_trace_atom_internal( self, objptr.get(), attrptr.get() );
    Py_RETURN_NONE;
}


PyObject*
StandardTracer_load_attr( StandardTracer* self, PyObject* args, PyObject* kwargs  )
{
    PyObject* obj;
    PyObject* attr;
    static char* kwlist[] = { "obj", "attr", 0 };
    if( !PyArg_ParseTupleAndKeywords( args, kwargs, "OU", kwlist, &obj, &attr ) )
        return 0;
    cppy::ptr objptr( cppy::incref(obj) );
    cppy::ptr attrptr( cppy::incref(attr) );
    if ( is_atom_instance( objptr.get() ) )
        return _StandardTracer_trace_atom_internal( self, objptr.get(), attrptr.get() );
    Py_RETURN_NONE;
}


PyObject*
StandardTracer_call_function( StandardTracer* self, PyObject* args, PyObject* kwargs  )
{
    PyObject* func;
    PyObject* argtuple;
    PyObject* argspec;
    static char* kwlist[] = { "func", "argtuple", "argspec", 0 };
    if( !PyArg_ParseTupleAndKeywords( args, kwargs, "OOi", kwlist, &func, &argtuple, &argspec ) )
        return 0;
    cppy::ptr funcptr( cppy::incref(func) );
    cppy::ptr argtupleptr( cppy::incref(argtuple) );
    if (
        is_getattr( funcptr.get() )
        && PyTuple_Check(argtupleptr.get())
        && PyTuple_Size(argtupleptr.get()) >= 2
    )
    {
        cppy::ptr obj( cppy::incref( PyTuple_GET_ITEM(argtupleptr.get(), 0) ) );
        cppy::ptr attr( cppy::incref( PyTuple_GET_ITEM(argtupleptr.get(), 1) ) );
        if ( is_atom_instance( obj.get() ) && PyUnicode_Check( attr.get() ) )
            return _StandardTracer_trace_atom_internal( self, obj.get(), attr.get() );

    }

    Py_RETURN_NONE;
}


PyObject*
StandardTracer_binary_subscr( StandardTracer* self, PyObject* args, PyObject*kwargs )
{
    Py_RETURN_NONE;
}

PyObject*
StandardTracer_get_iter( StandardTracer* self, PyObject* obj )
{
    Py_RETURN_NONE;
}


PyObject*
StandardTracer_return_value( StandardTracer* self, PyObject* value )
{
    return StandardTracer_finalize( self );
}


PyObject*
StandardTracer_richcompare( StandardTracer* self, PyObject* other, int opid )
{
    if( opid == Py_EQ )
    {
        if( StandardTracer::TypeCheck( other ) )
        {
            StandardTracer* so_other = reinterpret_cast<StandardTracer*>( other );
            cppy::ptr sowner( cppy::incref( self->owner ) );
            cppy::ptr sname( cppy::incref( self->name ) );
            cppy::ptr skey( cppy::incref( self->key ) );
            cppy::ptr sitems( cppy::incref( self->items ) );
            cppy::ptr oowner( cppy::incref( so_other->owner ) );
            cppy::ptr oname( cppy::incref( so_other->name ) );
            cppy::ptr okey( cppy::incref( so_other->key ) );
            cppy::ptr oitems( cppy::incref( so_other->items ) );
            if( sowner.richcmp( oowner, Py_EQ )
                && sname.richcmp( oname, Py_EQ )
                && sname.richcmp(oname, Py_EQ )
                && skey.richcmp(okey, Py_EQ )
                && sitems.richcmp(oitems, Py_EQ ) )
            {
                Py_RETURN_TRUE;
            }
        }
        Py_RETURN_FALSE;
    }
    Py_RETURN_NOTIMPLEMENTED;
}


PyObject*
StandardTracer_get_owner( StandardTracer* self, void* context )
{
    return cppy::incref( self->owner );
}


PyObject*
StandardTracer_set_owner( StandardTracer* self, PyObject* value, void* context )
{
    if( reinterpret_cast<PyObject*>( self ) == value )
        return 0;
    cppy::ptr old( self->owner );
    self->owner = cppy::incref( value );
    return 0;
}


PyObject*
StandardTracer_get_name( StandardTracer* self, void* context )
{
    return cppy::incref( self->name );
}

PyObject*
StandardTracer_set_name( StandardTracer* self, PyObject* value, void* context )
{
    if( reinterpret_cast<PyObject*>( self ) == value )
        return 0;
    if ( !PyUnicode_Check(value) )
        return cppy::type_error("name must be a str");
    cppy::ptr old( self->name );
    self->name = cppy::incref( value );
    return 0;
}


PyObject*
StandardTracer_get_key( StandardTracer* self, void* context )
{
    return cppy::incref( self->key );
}


PyObject*
StandardTracer_set_key( StandardTracer* self, PyObject* value, void* context )
{
    if( reinterpret_cast<PyObject*>( self ) == value )
        return 0;
    if ( !PyUnicode_Check(value) )
        return cppy::type_error("key must be a str");
    cppy::ptr old( self->key );
    self->key = cppy::incref( value );
    return 0;
}

PyObject*
StandardTracer_get_items( StandardTracer* self, void* context )
{
    return cppy::incref( self->items );
}


PyObject*
StandardTracer_set_items( StandardTracer* self, PyObject* value, void* context )
{
    if( reinterpret_cast<PyObject*>( self ) == value )
        return 0;
    if ( !PySet_Check(value) )
        return cppy::type_error("items must be a set");
    cppy::ptr old( self->items );
    self->items = cppy::incref( value );
    return 0;
}

PyDoc_STRVAR(StandardTracer__doc__,
             "StandardTracer(owner, name)\n\n"
             "A CodeTracer for tracing expressions which use Atom.\n"
             "This tracer maintains a running set of `traced_items` which are the\n"
             "(obj, name) pairs of atom items discovered during tracing.\n"
             "Parameters\n"
             "----------\n"
             "owner : Declarative\n"
             "    The declarative owner of interest.\n\n"
             "name : string\n"
             "    The name to which the tracer is bound\n");

static PyGetSetDef
StandardTracer_getset[] = {
    { "owner", ( getter )StandardTracer_get_owner, ( setter )StandardTracer_set_owner,
        "Get and set the owner for the tracer." },
    { "name", ( getter )StandardTracer_get_name, ( setter )StandardTracer_set_name,
        "Get and set the name for the tracer." },
    { "key", ( getter )StandardTracer_get_key, ( setter )StandardTracer_set_key,
        "Get and set the key for the tracer." },
    { "items", ( getter )StandardTracer_get_items, ( setter )StandardTracer_set_items,
        "Get and set the items for the tracer." },
    { 0 } // sentinel
};


static PyMethodDef
StandardTracer_methods[] = {
    { "trace_atom", ( PyCFunction )StandardTracer_trace_atom, METH_VARARGS | METH_KEYWORDS,
      "Get whether notification is enabled for the atom.\n"
      "\n"
      "Parameters\n"
      "----------\n"
      "obj : Atom\n"
      "The atom object owning the attribute.\n"
      "\n"
      "name : string\n"
      "The member name for which to bind a handler."
    },
    { "finalize", ( PyCFunction )StandardTracer_finalize, METH_NOARGS,
        "Finalize the tracing process.\n"
        "\n"
        "This method will discard the old observer and attach a new\n"
        "observer to the traced dependencies."
    },
    { "dynamic_load", ( PyCFunction )StandardTracer_dyanmic_load, METH_VARARGS | METH_KEYWORDS,
        "Called when an object attribute is dynamically loaded.\n"
        "\n"
        "This will trace the object if it is an Atom instance.\n"
        "See also: `CodeTracer.dynamic_load`."
    },
    { "load_attr", ( PyCFunction )StandardTracer_load_attr, METH_VARARGS | METH_KEYWORDS,
        "Called before the LOAD_ATTR opcode is executed.\n"
        "\n"
        "This will trace the object if it is an Atom instance.\n"
        "See also: `CodeTracer.load_attr`."
    },
    { "call_function", ( PyCFunction )StandardTracer_call_function, METH_VARARGS | METH_KEYWORDS,
        "Called before the CALL opcode is executed.\n"
        "\n"
        "This will trace the func if it is the builtin `getattr` and the\n"
        "object is an Atom instance. See also: `CodeTracer.call_function`"
    },
    { "binary_subscr", ( PyCFunction )StandardTracer_binary_subscr, METH_VARARGS,
        "Called before the BINARY_SUBSCR opcode is executed.\n"
    },
    { "get_iter", ( PyCFunction )StandardTracer_get_iter, METH_O,
        "Called before the GET_ITER opcode is executed.\n"
    },
    { "return_value", ( PyCFunction )StandardTracer_return_value, METH_O,
        "Called before the RETURN_VALUE opcode is executed.\n"
        "\n"
        "This handler finalizes the subscription.\n"
    },
    { 0 } // sentinel
};


static PyType_Slot StandardTracer_Type_slots[] = {
    { Py_tp_dealloc, void_cast( StandardTracer_dealloc ) },          /* tp_dealloc */
    { Py_tp_traverse, void_cast( StandardTracer_traverse) },         /* tp_traverse */
    { Py_tp_clear, void_cast( StandardTracer_clear ) },              /* tp_clear */
    { Py_tp_doc, cast_py_tp_doc( StandardTracer__doc__ ) },          /* tp_doc */
    { Py_tp_richcompare, void_cast( StandardTracer_richcompare ) },  /* tp_richcompare */
    { Py_tp_methods, void_cast( StandardTracer_methods ) },          /* tp_methods */
    { Py_tp_getset, void_cast( StandardTracer_getset ) },            /* tp_getset */
    { Py_tp_new, void_cast( StandardTracer_new ) },                  /* tp_new */
    { Py_tp_alloc, void_cast( PyType_GenericAlloc ) },                     /* tp_alloc */
    { 0, 0 },
};


}  // namespace


// Initialize static variables (otherwise the compiler eliminates them)
PyTypeObject* StandardTracer::TypeObject = NULL;


PyType_Spec StandardTracer::TypeObject_Spec = {
    "enaml.core.standard_tracer.StandardTracer",     /* tp_name */
    sizeof( StandardTracer ),               /* tp_basicsize */
    0,                                   /* tp_itemsize */
    Py_TPFLAGS_DEFAULT
    |Py_TPFLAGS_BASETYPE
    |Py_TPFLAGS_HAVE_GC,                 /* tp_flags */
    StandardTracer_Type_slots               /* slots */
};


bool StandardTracer::Ready()
{
    // The reference will be handled by the module to which we will add the type
    TypeObject = pytype_cast( PyType_FromSpec( &TypeObject_Spec ) );
    if( !TypeObject )
    {
        return false;
    }
    return true;
}


bool StandardTracer::TypeCheck( PyObject* ob )
{
    return PyObject_TypeCheck( ob, TypeObject ) != 0;
}


// Module definition
namespace
{


int
standard_tracer_modexec( PyObject *mod )
{
    if( !StandardTracer::Ready() )
    {
        return -1;
    }

    // standard_tracer
    cppy::ptr standard_tracer( pyobject_cast(  StandardTracer::TypeObject ) );
    if( PyModule_AddObject( mod, "StandardTracer", standard_tracer.get() ) < 0 )
    {
        return -1;
    }
    standard_tracer.release();

    return 0;
}


PyMethodDef
standard_tracer_methods[] = {
    { 0 } // Sentinel
};


PyModuleDef_Slot standard_tracer_slots[] = {
    {Py_mod_exec, reinterpret_cast<void*>( standard_tracer_modexec ) },
    {0, NULL}
};


struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "standard_tracer",
    "standard_tracer extension module",
    0,
    standard_tracer_methods,
    standard_tracer_slots,
    NULL,
    NULL,
    NULL
};


}  // module namespace


}  // namespace enaml


PyMODINIT_FUNC PyInit_standard_tracer( void )
{
    return PyModuleDef_Init( &enaml::moduledef );
}
