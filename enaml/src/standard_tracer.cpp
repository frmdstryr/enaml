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

static PyObject* atomref;
static PyObject* getattr;
static PyObject* Atom;
static PyObject* Alias;
static PyObject* get_member_str;
static PyObject* d_engine_str;
static PyObject* d_storage_str;
static PyObject* update_str;
static PyObject* resolve_str;
static PyObject* ref_str;
static PyObject* observe_str;

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

// POD struct - all member fields are considered private
struct SubscriptionObserver
{
    PyObject_HEAD
    PyObject* ref;
    PyObject* name;

    static PyType_Spec TypeObject_Spec;
    static PyTypeObject* TypeObject;

    static bool Ready();
    static bool TypeCheck( PyObject* ob );

};

namespace {



PyObject*
SubscriptionObserver_new( PyTypeObject* type, PyObject* args, PyObject* kwargs )
{
    PyObject* owner;
    PyObject* name;
    static char* kwlist[] = { "owner", "name", 0 };
    if( !PyArg_ParseTupleAndKeywords( args, kwargs, "OU", kwlist, &owner, &name ) )
        return 0;

    cppy::ptr ptr( PyType_GenericNew( type, args, kwargs ) );
    if( !ptr )
        return 0;

    SubscriptionObserver* self = reinterpret_cast<SubscriptionObserver*>( ptr.get() );

    self->ref = PyObject_CallOneArg(atomref, owner);
    if( !self->ref )
        return 0;
    self->name =  cppy::incref( name );
    return ptr.release();
}


void
SubscriptionObserver_clear( SubscriptionObserver* self )
{
    Py_CLEAR( self->ref );
    Py_CLEAR( self->name );
}


int
SubscriptionObserver_traverse( SubscriptionObserver* self, visitproc visit, void* arg )
{
    Py_VISIT( self->ref );
    return 0;
}


void
SubscriptionObserver_dealloc( SubscriptionObserver* self )
{
    PyObject_GC_UnTrack( self );
    SubscriptionObserver_clear( self );
    Py_TYPE(self)->tp_free( reinterpret_cast<PyObject*>( self ) );
}


int
SubscriptionObserver__bool__( SubscriptionObserver* self )
{
    return PyObject_IsTrue( self->ref );
}


/*
    * Calls engine update with the owner and name
    * if self.ref:
    *     owner = self.ref()
    *     engine = owner._d_engine
    *      if engine is not None:
    *         engine.update(owner, self.name)
    */
PyObject*
SubscriptionObserver_call( SubscriptionObserver* self, PyObject* args, PyObject* kwargs )
{

    if( PyObject_IsTrue( self->ref ) )
    {
        cppy::ptr owner( PyObject_CallNoArgs( self->ref ) );
        if ( !owner )
            return 0;
        cppy::ptr engine( owner.getattr(d_engine_str) );
        if ( !engine )
            return 0;
        if ( !engine.is_none() )
        {
            PyObject* call_args[] = { engine.get(), owner.get(), self->name };
            return PyObject_VectorcallMethod(update_str, call_args, 3 | PY_VECTORCALL_ARGUMENTS_OFFSET, 0);
        }
    }
    Py_RETURN_NONE;
}


PyDoc_STRVAR(SubscriptionObserver__doc__,
                "SubscriptionObserver(owner, name)\n\n"
                "An observer object which manages a tracer subscription.\n"
                "Parameters\n"
                "----------\n"
                "owner : Declarative\n"
                "    The declarative owner of interest.\n\n"
                "name : string\n"
                "    The name to which the operator is bound\n");


PyObject*
SubscriptionObserver_get_ref( SubscriptionObserver* self, void* context )
{
    return cppy::incref( self->ref );
}


PyObject*
SubscriptionObserver_set_ref( SubscriptionObserver* self, PyObject* value, void* context )
{
    if( value != Py_None )
        return cppy::type_error("ref can only be set to None");
    cppy::replace( &self->ref, Py_None );
    return 0;
}


PyObject*
SubscriptionObserver_get_name( SubscriptionObserver* self, void* context )
{
    return cppy::incref( self->name );
}


static PyGetSetDef
SubscriptionObserver_getset[] = {
    { "ref", ( getter )SubscriptionObserver_get_ref, ( setter )SubscriptionObserver_set_ref,
        "Get and set the ref for the observer." },
    { "name", ( getter )SubscriptionObserver_get_name, 0,
        "Get the name for the observer." },
    { 0 } // sentinel
};


static PyType_Slot SubscriptionObserver_Type_slots[] = {
    { Py_tp_dealloc, void_cast( SubscriptionObserver_dealloc ) },          /* tp_dealloc */
    { Py_tp_traverse, void_cast( SubscriptionObserver_traverse) },         /* tp_traverse */
    { Py_tp_clear, void_cast( SubscriptionObserver_clear ) },              /* tp_clear */
    { Py_tp_call, void_cast( SubscriptionObserver_call ) },                /* tp_call */
    { Py_tp_doc, cast_py_tp_doc( SubscriptionObserver__doc__ ) },          /* tp_doc */
    { Py_nb_bool, void_cast( SubscriptionObserver__bool__ ) },             /* nb_bool */
    { Py_tp_getset, void_cast( SubscriptionObserver_getset ) },            /* tp_getset */
    { Py_tp_new, void_cast( SubscriptionObserver_new ) },                  /* tp_new */
    { Py_tp_alloc, void_cast( PyType_GenericAlloc ) },                     /* tp_alloc */
    { 0, 0 },
};

} // namespace

// Initialize static variables (otherwise the compiler eliminates them)
PyTypeObject* SubscriptionObserver::TypeObject = NULL;


PyType_Spec SubscriptionObserver::TypeObject_Spec = {
    "enaml.core.standard_tracer.SubscriptionObserver",     /* tp_name */
    sizeof( SubscriptionObserver ),               /* tp_basicsize */
    0,                                   /* tp_itemsize */
    Py_TPFLAGS_DEFAULT
    |Py_TPFLAGS_BASETYPE
    |Py_TPFLAGS_HAVE_GC,                 /* tp_flags */
    SubscriptionObserver_Type_slots               /* slots */
};


bool SubscriptionObserver::Ready()
{
    // The reference will be handled by the module to which we will add the type
    TypeObject = pytype_cast( PyType_FromSpec( &TypeObject_Spec ) );
    if( !TypeObject )
        return false;
    return true;
}


bool SubscriptionObserver::TypeCheck( PyObject* ob )
{
    return PyObject_TypeCheck( ob, TypeObject ) != 0;
}


namespace {


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
    Py_VISIT(Py_TYPE(self));
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
    const int r =  PyObject_IsInstance( obj, Alias );
    if (r < 0 )
    {
        PyErr_Clear();
        return 0;
    }
    return r;
}

static bool is_atom_instance( PyObject* obj )
{
    const int r =  PyObject_IsInstance( obj, Atom );
    if (r < 0 )
    {
        PyErr_Clear();
        return 0;
    }
    return r;
}


static bool is_getattr( PyObject* obj )
{
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

    PyObject* get_member_args[] = { obj, name };
    cppy::ptr member( PyObject_VectorcallMethod(
        get_member_str,
        get_member_args,
        2 | PY_VECTORCALL_ARGUMENTS_OFFSET,
        0
    ) );
    if ( !member )
        return 0;
    if ( !member.is_none() )
    {
        cppy::ptr item( PyTuple_New (2) );
        if( !item )
            return 0;
        PyTuple_SET_ITEM( item.get(), 0, cppy::incref( obj ) );
        PyTuple_SET_ITEM( item.get(), 1, cppy::incref( name ) );
        if ( PySet_Add( self->items, item.get()) )
            return 0;
    }
    else {
        cppy::ptr objtype( PyObject_Type( obj ) );
        if ( !objtype )
            return 0;
        cppy::ptr aliasptr( objtype.getattr( name ) );
        if ( !aliasptr && PyErr_Occurred() )
            PyErr_Clear(); // getattr(type(obj), name) is None
        else if ( is_alias( aliasptr.get() ) ) {
            PyObject* resolve_args[] = { aliasptr.get(), obj };
            cppy::ptr alias_result( PyObject_VectorcallMethod(
                resolve_str,
                resolve_args,
                2 | PY_VECTORCALL_ARGUMENTS_OFFSET,
                0
            ) );
            if ( !alias_result )
                return 0;
            if ( !PyTuple_Check(alias_result.get()) || PyTuple_GET_SIZE(alias_result.get()) != 2 )
                return cppy::type_error("alias resolve should return tuple of (obj, attr");
            PyObject* alias_obj = PyTuple_GET_ITEM( alias_result.get(), 0 );
            PyObject* alias_attr = PyTuple_GET_ITEM( alias_result.get(), 1 );
            if ( alias_attr != Py_None )
                return _StandardTracer_trace_atom_internal(self, alias_obj, alias_attr);
        }
    }
    Py_RETURN_NONE;
}

PyObject*
StandardTracer_trace_atom( StandardTracer* self, PyObject *const *args, Py_ssize_t nargs )
{
    // obj, attr
    if ( nargs != 2 )
        return cppy::type_error("trace_atom requires 2 args: obj, attr");
    if ( !is_atom_instance( args[0] ) )
        return cppy::type_error("trace_atom first argument must be an atom instance");
    if ( !PyUnicode_Check( args[1] ) )
        return cppy::type_error("trace_atom first argument must be a str");
    return _StandardTracer_trace_atom_internal(self, args[0], args[1]);
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
    cppy::ptr storage( PyObject_GetAttr( self->owner, d_storage_str ) );
    if ( !storage )
        return 0;

    // invalidate the old observer so that it can be collected
    cppy::ptr old_observer( PyObject_GetItem( storage.get(), self->key ) );
    if ( !old_observer && PyErr_Occurred() )
        PyErr_Clear();
    else
        old_observer.setattr(ref_str, Py_None);

    if ( PyObject_IsTrue( self->items ) )
    {
        PyObject* observer_args[] = { self->owner, self->name };
        cppy::ptr observer( PyObject_Vectorcall( pyobject_cast(SubscriptionObserver::TypeObject), observer_args, 2, 0 ) );
        if ( !observer )
            return 0;

        if ( PyObject_SetItem( storage.get(), self->key, observer.get() ) )
            return 0;

        cppy::ptr item;
        cppy::ptr iter( PyObject_GetIter( self->items ) );
        if ( !iter )
            return 0;

        while ( (item = iter.next())  )
        {
            if ( !PyTuple_Check(item.get()) || PyTuple_GET_SIZE(item.get()) != 2 )
                return cppy::type_error("StandardTracer items should be a tuple of (obj, d_name)");
            PyObject* obj = PyTuple_GET_ITEM(item.get(), 0);
            PyObject* d_name = PyTuple_GET_ITEM(item.get(), 1);
            PyObject* observe_args[] = { obj, d_name, observer.get() };
            cppy::ptr result( PyObject_VectorcallMethod(observe_str, observe_args, 3 | PY_VECTORCALL_ARGUMENTS_OFFSET, 0 ) );
            if ( !result )
                return 0;
        }
    }
    Py_RETURN_NONE;
}


PyObject*
StandardTracer_dyanmic_load( StandardTracer* self, PyObject*const *args, Py_ssize_t nargs )
{
    if ( nargs != 3 )
        return cppy::type_error("dyanmic_load requires 3 args: obj, attr, value");
    if ( is_atom_instance( args[0] ) && PyUnicode_Check( args[1] ) )
        return _StandardTracer_trace_atom_internal( self, args[0], args[1] );
    Py_RETURN_NONE;
}


PyObject*
StandardTracer_load_attr( StandardTracer* self, PyObject*const *args, Py_ssize_t nargs )
{
    if ( nargs != 2 )
        return cppy::type_error("load_attr requires 2 args: obj, attr");
    if ( is_atom_instance( args[0] ) && PyUnicode_Check( args[1] ) )
        return _StandardTracer_trace_atom_internal( self, args[0], args[1] );
    Py_RETURN_NONE;
}


PyObject*
StandardTracer_call_function( StandardTracer* self, PyObject*const *args, Py_ssize_t nargs  )
{
    if ( nargs != 3 )
        return cppy::type_error("call_function requires 3 args: func, argtuple, argspec");
    PyObject* argtuple = args[1];
    if ( is_getattr( args[0] ) && PyTuple_Check(argtuple) && PyTuple_GET_SIZE(argtuple) >= 2 )
    {
        PyObject* obj = PyTuple_GET_ITEM(argtuple, 0);
        PyObject* attr = PyTuple_GET_ITEM(argtuple, 1);
        if ( is_atom_instance( obj ) && PyUnicode_Check( attr ) )
            return _StandardTracer_trace_atom_internal( self, obj, attr );
    }

    Py_RETURN_NONE;
}


PyObject*
StandardTracer_binary_subscr( StandardTracer* self, PyObject*const *args, Py_ssize_t nargs )
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
StandardTracer_get_owner( StandardTracer* self, void* context )
{
    return cppy::incref( self->owner );
}

PyObject*
StandardTracer_get_name( StandardTracer* self, void* context )
{
    return cppy::incref( self->name );
}


PyObject*
StandardTracer_get_key( StandardTracer* self, void* context )
{
    return cppy::incref( self->key );
}


PyObject*
StandardTracer_get_items( StandardTracer* self, void* context )
{
    return cppy::incref( self->items );
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
    { "owner", ( getter )StandardTracer_get_owner, 0,
        "Get and set the owner for the tracer." },
    { "name", ( getter )StandardTracer_get_name, 0,
        "Get and set the name for the tracer." },
    { "key", ( getter )StandardTracer_get_key, 0,
        "Get and set the key for the tracer." },
    { "items", ( getter )StandardTracer_get_items, 0,
        "Get and set the items for the tracer." },
    { 0 } // sentinel
};


static PyMethodDef
StandardTracer_methods[] = {
    { "trace_atom", ( PyCFunction )StandardTracer_trace_atom, METH_FASTCALL,
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
    { "dynamic_load", ( PyCFunction )StandardTracer_dyanmic_load, METH_FASTCALL,
        "Called when an object attribute is dynamically loaded.\n"
        "\n"
        "This will trace the object if it is an Atom instance.\n"
        "See also: `CodeTracer.dynamic_load`."
    },
    { "load_attr", ( PyCFunction )StandardTracer_load_attr, METH_FASTCALL,
        "Called before the LOAD_ATTR opcode is executed.\n"
        "\n"
        "This will trace the object if it is an Atom instance.\n"
        "See also: `CodeTracer.load_attr`."
    },
    { "call_function", ( PyCFunction )StandardTracer_call_function, METH_FASTCALL,
        "Called before the CALL opcode is executed.\n"
        "\n"
        "This will trace the func if it is the builtin `getattr` and the\n"
        "object is an Atom instance. See also: `CodeTracer.call_function`"
    },
    { "binary_subscr", ( PyCFunction )StandardTracer_binary_subscr, METH_FASTCALL,
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
        return false;
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
    if( !StandardTracer::Ready() || !SubscriptionObserver::Ready() )
        return -1;


    d_engine_str = PyUnicode_FromString("_d_engine");
    if ( !d_engine_str )
        return -1;

    d_storage_str = PyUnicode_FromString("_d_storage");
    if ( !d_storage_str )
        return -1;

    update_str = PyUnicode_FromString("update");
    if ( !update_str )
        return -1;

    observe_str = PyUnicode_FromString("observe");
    if ( !observe_str )
        return -1;

    resolve_str = PyUnicode_FromString("resolve");
    if ( !resolve_str )
        return -1;

    ref_str = PyUnicode_FromString("ref");
    if ( !ref_str )
        return -1;

    get_member_str = PyUnicode_FromString("get_member");
    if ( !get_member_str )
        return -1;

    cppy::ptr atom_api( PyImport_ImportModule("atom.api") );
    if ( !atom_api )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import atom.api" );
        return -1;
    }
    atomref = atom_api.getattr("atomref");
    if ( !atomref )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import atom.api.atomref" );
        return 0;
    }

    Atom = atom_api.getattr("Atom");
    if ( !Atom )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import atom.api.Atom" );
        return 0;
    }

    cppy::ptr builtins( PyImport_ImportModule("builtins") );
    if ( !builtins )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import builtins" );
        return -1;
    }

    getattr = builtins.getattr("getattr");
    if ( !getattr )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import builtins.getattr" );
        return 0;
    }

    cppy::ptr enaml_core_alias( PyImport_ImportModule("enaml.core.alias") );
    if ( !enaml_core_alias )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import enaml.core.Alias" );
        return -1;
    }
    Alias = enaml_core_alias.getattr("Alias");
    if ( !Alias )
    {
        PyErr_SetString( PyExc_ImportError, "Could not import enaml.core.alias.Alias" );
        return 0;
    }

    cppy::ptr standard_tracer( pyobject_cast(  StandardTracer::TypeObject ) );
    if( PyModule_AddObject( mod, "StandardTracer", standard_tracer.get() ) < 0 )
        return -1;
    standard_tracer.release();

    cppy::ptr subscription_observer( pyobject_cast(  SubscriptionObserver::TypeObject ) );
    if( PyModule_AddObject( mod, "SubscriptionObserver", subscription_observer.get() ) < 0 )
        return -1;
    subscription_observer.release();

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
