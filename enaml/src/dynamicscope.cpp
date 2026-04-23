/*-----------------------------------------------------------------------------
| Copyright (c) 2013-2025, Nucleic Development Team.
|
| Distributed under the terms of the Modified BSD License.
|
| The full license is in the file LICENSE, distributed with this software.
|----------------------------------------------------------------------------*/
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
struct Nonlocals
{
	PyObject_HEAD
    PyObject* owner;
    PyObject* tracer;

	static PyType_Spec TypeObject_Spec;

    static PyTypeObject* TypeObject;

	static bool Ready();

};


// POD struct - all member fields are considered private
struct DynamicScope
{
	PyObject_HEAD
    PyObject* owner;
    PyObject* change;
    PyObject* tracer;
    PyObject* f_locals;
    PyObject* f_globals;
    PyObject* f_builtins;
    PyObject* f_writes;
    PyObject* f_nonlocals;

	static PyType_Spec TypeObject_Spec;

    static PyTypeObject* TypeObject;

	static bool Ready();

};


namespace
{


static PyObject* parent_str;
static PyObject* dynamic_load_str;
static PyObject* d_storage_str;
static PyObject* get_str;
static PyObject* super_disallowed;
static PyObject* UserKeyError;


/*-----------------------------------------------------------------------------
| Utilities
|----------------------------------------------------------------------------*/
int
test_dynamic_attr( PyObject* obj, PyObject* name )
{
    PyTypeObject* tp;
    PyObject** dictptr;
    cppy::ptr descr;
    descrgetfunc descr_f;
    cppy::ptr objptr( cppy::incref( obj ) );

    // The body of this loop is PyObject_GenericGetAttr, modified to
    // use smart pointers and _PyObject_GetDictPtr, and only test for
    // the presence of descriptors, not evaluate them.
    while( objptr.get() != Py_None )
    {
        tp = Py_TYPE( objptr.get() );

        // Data descriptor
        descr_f = 0;
        descr = cppy::xincref( _PyType_Lookup( tp, name ) );
        if( descr )
        {
            descr_f = descr.type()->tp_descr_get;
            if( descr_f && PyDescr_IsData( descr.get() ) )
                return 1;
        }

        // Instance dictionary
        dictptr = _PyObject_GetDictPtr( objptr.get() );
        if( dictptr && *dictptr )
        {
            if( PyDict_GetItem( *dictptr, name ) )
                return 1;
        }

        // Non-data descriptor
        if( descr_f || descr )
            return 1;

        // Step up to the parent object
        objptr = PyObject_GetAttr( objptr.get(), parent_str );
        if( !objptr )
            return -1;
    }

    return 0;
}


// The evaluation of a descriptor can trigger arbitrary code to execute,
// such as an expression bound to an attribute. If that expression raises
// a key error, it would be treated as a scope miss instead of reporting
// the failure in the user code. This function is used to convert such an
// error into a UserKeyError, which does not derive from KeyError and so
// will not be trapped by the Python VM during expression eval.
inline void
maybe_translate_key_error()
{
    if( PyErr_Occurred() && PyErr_ExceptionMatches( PyExc_KeyError ) )
    {
        PyObject* ptype;
        PyObject* pvalue;
        PyObject* ptraceback;
        PyErr_Fetch( &ptype, &pvalue, &ptraceback );
        PyErr_Restore( cppy::incref( UserKeyError ), pvalue, ptraceback );
        Py_DECREF( ptype );
    }
}


inline bool
run_tracer( PyObject* tracer, PyObject* owner, PyObject* name, PyObject* value )
{
    PyObject* args[] = {tracer, owner, name, value};
    cppy::ptr res( PyObject_VectorcallMethod( dynamic_load_str, args, 4 | PY_VECTORCALL_ARGUMENTS_OFFSET, 0) );
    if( !res )
        return false;
    return true;
}


PyObject*
load_dynamic_attr( PyObject* obj, PyObject* name, PyObject* tracer=0 )
{
    PyTypeObject* tp;
    PyObject** dictptr;
    cppy::ptr descr;
    descrgetfunc descr_f;
    cppy::ptr objptr( cppy::incref( obj ) );

    // The body of this loop is PyObject_GenericGetAttr, modified to
    // use smart pointers and _PyObject_GetDictPtr, and run a tracer.
    while( objptr.get() != Py_None )
    {
        tp = Py_TYPE( objptr.get() );

        // Data descriptor
        descr_f = 0;
        descr = cppy::xincref( _PyType_Lookup( tp, name ) );
        if( descr )
        {
            descr_f = descr.type()->tp_descr_get;
            if( descr_f && PyDescr_IsData( descr.get() ) )
            {
                cppy::ptr res(
                    descr_f( descr.get(), objptr.get(), pyobject_cast( tp ) )
                );
                if( !res )
                    maybe_translate_key_error();
                else if( tracer && !run_tracer( tracer, objptr.get(), name, res.get() ) )
                    return 0;
                return res.release();
            }
        }

        // Instance dictionary
        dictptr = _PyObject_GetDictPtr( objptr.get() );
        if( dictptr && *dictptr )
        {
            PyObject* item = PyDict_GetItem( *dictptr, name );
            if( item )
            {
                if( tracer && !run_tracer( tracer, objptr.get(), name, item ) )
                    return 0;
                return cppy::incref( item );
            }
        }

        // Non-data descriptor
        if( descr_f )
        {
            cppy::ptr res(
                descr_f( descr.get(), objptr.get(), pyobject_cast( tp ) )
            );
            if( !res )
                maybe_translate_key_error();
            else if( tracer && !run_tracer( tracer, objptr.get(), name, res.get() ) )
                return 0;
            return res.release();
        }

        // Non-readable descriptor
        if( descr )
        {
            if( tracer && !run_tracer( tracer, objptr.get(), name, descr.get() ) )
                return 0;
            return descr.release();
        }

        // Step up to the parent object
        objptr = PyObject_GetAttr( objptr.get(), parent_str );
        if( !objptr )
            return 0;
    }

    return 0;
}


int
set_dynamic_attr( PyObject* obj, PyObject* name, PyObject* value )
{
    PyTypeObject* tp;
    PyObject* dict;
    PyObject** dictptr;
    cppy::ptr descr;
    descrsetfunc descr_f;
    cppy::ptr objptr( cppy::incref( obj ) );

    // The body of this loop is PyObject_GenericGetAttr, modified to
    // use smart pointers.
    while( objptr.get() != Py_None )
    {
        tp = Py_TYPE( objptr.get() );

        // Data desciptor
        descr_f = 0;
        descr = cppy::xincref( _PyType_Lookup( tp, name ) );
        if( descr )
        {
            descr_f = descr.type()->tp_descr_set;
            if( descr_f && PyDescr_IsData( descr.get() ) )
                return descr_f( descr.get(), objptr.get(), value );
        }

        // Instance dictionary
        dict = 0;
        dictptr = _PyObject_GetDictPtr( obj );
        if( dictptr )
        {
            dict = *dictptr;
            if( !dict && value )
            {
                dict = PyDict_New();
                if( !dict )
                    return -1;
                *dictptr = dict;
            }
        }
        if( dict )
        {
            if( value )
                return PyDict_SetItem( dict, name, value );
            if( PyDict_DelItem( dict, name ) == 0 )
                return 0;
            if( !PyErr_ExceptionMatches( PyExc_KeyError ) )
                return -1;
            PyErr_Clear();
        }

        // Non-data descriptor
        if( descr_f )
            return descr_f( descr.get(), objptr.get(), value );

        // Read-only descriptor
        if( descr )
            PyErr_Format(
                PyExc_AttributeError,
                "'%.50s' object attribute '%.400s' is read-only",
                tp->tp_name, PyUnicode_AsUTF8( name )
            );

        // Step up to the parent object
        objptr = PyObject_GetAttr( objptr.get(), parent_str );
        if( !objptr )
            return -1;
    }

    return -1;
}


PyObject*
_SuperDisallowed( PyObject* mod, PyObject* args, PyObject* kwargs)
{
    return cppy::type_error( "super() is not allowed in a declarative function, "
    " use SomeClass.some_method(self, ...) instead." );
}


/*-----------------------------------------------------------------------------
| Nonlocals
|----------------------------------------------------------------------------*/
void
Nonlocals_clear( Nonlocals* self )
{
    Py_CLEAR( self->owner );
    Py_CLEAR( self->tracer );
}


int
Nonlocals_traverse( Nonlocals* self, visitproc visit, void* arg )
{
    Py_VISIT( self->owner );
    Py_VISIT( self->tracer );
    Py_VISIT(Py_TYPE(self));
    return 0;
}


void
Nonlocals_dealloc( Nonlocals* self )
{
    PyTypeObject *tp = Py_TYPE(self);
    PyObject_GC_UnTrack( self );
    Nonlocals_clear( self );
    tp->tp_free( pyobject_cast( self ) );
    Py_DECREF(tp);
}


PyObject*
Nonlocals_repr( Nonlocals* self )
{
    cppy::ptr pystr( PyObject_Str( self->owner ) );
    if( !pystr )
        return 0;
    return PyUnicode_FromFormat(
        "%s[%s]",
        Py_TYPE(self)->tp_name,
        PyUnicode_AsUTF8( pystr.get() )
    );
}


PyObject*
Nonlocals_call( Nonlocals* self, PyObject* args, PyObject* kwargs )
{
    unsigned int level;
    static char* kwlist[] = { "level", 0 };
    if( !PyArg_ParseTupleAndKeywords( args, kwargs, "I", kwlist, &level ) )
        return 0;
    unsigned int offset = 0;
    cppy::ptr parentptr;
    cppy::ptr objptr( cppy::incref( self->owner ) );
    while( offset != level )
    {
        parentptr = PyObject_GetAttr( objptr.get(), parent_str );
        if( !parentptr )
            return 0;
        if( parentptr.get() == Py_None )
            break;
        objptr = parentptr;
        ++offset;
    }
    if( offset != level )
    {
        PyErr_Format( PyExc_ValueError, "Scope level %u is out of range", level );
        return 0;
    }
    PyObject* res = PyType_GenericNew( Py_TYPE(self), 0, 0 );
    if( !res )
        return 0;
    Nonlocals* nl = reinterpret_cast<Nonlocals*>( res );
    nl->owner = cppy::incref( objptr.get() );
    nl->tracer = cppy::xincref( self->tracer );
    return res;
}


PyObject*
Nonlocals_getattro( Nonlocals* self, PyObject* name )
{
    PyObject* res = load_dynamic_attr( self->owner, name, self->tracer );
    if( !res && !PyErr_Occurred() )
        PyErr_Format(
            PyExc_AttributeError,
            "'%.50s' object has no attribute '%.400s'",
            Py_TYPE(self)->tp_name,
            PyUnicode_AsUTF8( name )
        );
    return res;
}


int
Nonlocals_setattro( Nonlocals* self, PyObject* name, PyObject* value )
{
    int res = set_dynamic_attr( self->owner, name, value );
    if( res < 0 && !PyErr_Occurred() )
        PyErr_Format(
            PyExc_AttributeError,
            "'%.50s' object has no attribute '%.400s'",
            Py_TYPE(self)->tp_name,
            PyUnicode_AsUTF8( name )
        );
    return res;
}


PyObject*
Nonlocals_getitem( Nonlocals* self, PyObject* key )
{
    if( !PyUnicode_CheckExact( key ) )
        return cppy::type_error( key, "str" );
    PyObject* res = load_dynamic_attr( self->owner, key, self->tracer );
    if( !res && !PyErr_Occurred() )
        PyErr_SetObject( PyExc_KeyError, key );
    return res;
}


int
Nonlocals_setitem( Nonlocals* self, PyObject* key, PyObject* value )
{
    if( !PyUnicode_CheckExact( key ) )
    {
        cppy::type_error( key, "str" );
        return -1;
    }
    int res = set_dynamic_attr( self->owner, key, value );
    if( res < 0 && !PyErr_Occurred() )
        PyErr_SetObject( PyExc_KeyError, key );
    return res;
}


int
Nonlocals_contains( Nonlocals* self, PyObject* key )
{
    if( !PyUnicode_CheckExact( key ) )
    {
        cppy::type_error( key, "str" );
        return -1;
    }
    return test_dynamic_attr( self->owner, key );
}


static PyType_Slot Nonlocals_Type_slots[] = {
    { Py_tp_dealloc, void_cast( Nonlocals_dealloc ) },        /* tp_dealloc */
    { Py_tp_traverse, void_cast( Nonlocals_traverse ) },      /* tp_traverse */
    { Py_tp_clear, void_cast( Nonlocals_clear ) },            /* tp_clear */
    { Py_tp_call, void_cast( Nonlocals_call ) },              /* tp_call */
    { Py_tp_repr, void_cast( Nonlocals_repr ) },              /* tp_repr */
    { Py_tp_getattro, void_cast( Nonlocals_getattro ) },      /* tp_getattro */
    { Py_tp_setattro, void_cast( Nonlocals_setattro ) },      /* tp_setattro */
    { Py_tp_alloc, void_cast( PyType_GenericAlloc ) },        /* tp_alloc */
    { Py_tp_free, void_cast( PyObject_GC_Del ) },             /* tp_free */
    { Py_mp_subscript, void_cast( Nonlocals_getitem ) },      /* mp_subscript */
    { Py_mp_ass_subscript, void_cast( Nonlocals_setitem ) },  /* mp_ass_subscript */
    { Py_sq_contains, void_cast( Nonlocals_contains ) },      /* sq_contains */
    { 0, 0 },
};


}  // namespace


// Initialize static variables (otherwise the compiler eliminates them)
PyTypeObject* Nonlocals::TypeObject = NULL;


PyType_Spec Nonlocals::TypeObject_Spec = {
	"enaml.dynamicscope.Nonlocals",    /* tp_name */
	sizeof( Nonlocals ),               /* tp_basicsize */
	0,                                 /* tp_itemsize */
	Py_TPFLAGS_DEFAULT|
    Py_TPFLAGS_HAVE_GC,                /* tp_flags */
    Nonlocals_Type_slots               /* slots */
};


bool Nonlocals::Ready()
{
    // The reference will be handled by the module to which we will add the type
	TypeObject = pytype_cast( PyType_FromSpec( &TypeObject_Spec ) );
    if( !TypeObject )
    {
        return false;
    }
    return true;
}


namespace
{


/*-----------------------------------------------------------------------------
| DynamicScope
|----------------------------------------------------------------------------*/
PyObject*
DynamicScope_new( PyTypeObject* type, PyObject* args, PyObject* kwargs )
{
    PyObject* owner;
    PyObject* func;
    PyObject* scope_key;
    PyObject* change = 0;
    PyObject* tracer = 0;
    if( !PyArg_ParseTuple( args, "OOO|OO:__new__", &owner, &func, &scope_key, &change, &tracer ) )
        return 0;
    if( !PyFunction_Check( func ) )
        return cppy::type_error( func, "function" );

    PyFunctionObject* f = reinterpret_cast<PyFunctionObject*>(func);
    PyObject* f_globals = f->func_globals;
    PyObject* f_builtins = f->func_builtins;

    if( !PyDict_CheckExact( f_globals ) )
        return cppy::type_error( f_globals, "dict" );
    if( f_builtins && !PyDict_CheckExact( f_builtins ) )
        return cppy::type_error( f_builtins, "dict" );

    cppy::ptr d_storage( PyObject_GetAttr( owner, d_storage_str ) );
    if ( !d_storage )
        return 0;
    cppy::ptr empty( PyDict_New() );
    PyObject* d_storage_get_args[] = { d_storage.get(), scope_key, empty.get() };
    cppy::ptr f_locals( PyObject_VectorcallMethod( get_str, d_storage_get_args, 2 | PY_VECTORCALL_ARGUMENTS_OFFSET, 0 ) );
    if ( !f_locals )
        return 0;

    PyObject* self = PyType_GenericNew( type, 0, 0 );
    if( !self )
        return 0;
    DynamicScope* scope = reinterpret_cast<DynamicScope*>( self );
    scope->owner = cppy::incref( owner );
    scope->f_locals = f_locals.release();
    scope->f_globals = cppy::incref( f_globals );
    scope->f_builtins = cppy::xincref( f_builtins );
    if( change && change != Py_None )
        scope->change = cppy::incref( change );
    if( tracer && tracer != Py_None )
        scope->tracer = cppy::incref( tracer );
    return self;
}

// PyObject*
// DynamicScope_vectorcall(
//     PyObject *type, PyObject *const *args, size_t nargsf, PyObject *kwnames
// )
// {
//     const auto n = PyVectorcall_NARGS(nargsf);
//     if ( n < 3 || n > 6 || kwnames)
//         return cppy::type_error("signature is DynamicScope(owner, f_locals, f_globals, [f_builtins, change, tracer])");
//     PyObject* owner = args[0];
//     PyObject* f_locals = args[1];
//     PyObject* f_globals = args[2];
//     PyObject* f_builtins = n > 3 ? args[3] : 0;
//     PyObject* change = n > 4 ? args[4] : Py_None;
//     PyObject* tracer = n > 5 ? args[5] : Py_None;
//     if( !PyMapping_Check( f_locals ) )
//         return cppy::type_error( f_locals, "mapping" );
//     if( !PyDict_CheckExact( f_globals ) )
//         return cppy::type_error( f_globals, "dict" );
//     if( f_builtins && !PyDict_CheckExact( f_builtins ) )
//         return cppy::type_error( f_builtins, "dict" );
//     PyObject* self = PyType_GenericNew( pytype_cast(type), 0, 0 );
//     if( !self )
//         return 0;
//     DynamicScope* scope = reinterpret_cast<DynamicScope*>( self );
//     scope->owner = cppy::incref( owner );
//     scope->f_locals = cppy::incref( f_locals );
//     scope->f_globals = cppy::incref( f_globals );
//     scope->f_builtins = cppy::xincref( f_builtins );
//     if( change != Py_None )
//         scope->change = cppy::incref( change );
//     if( tracer != Py_None )
//         scope->tracer = cppy::incref( tracer );
//     return self;
// }

void
DynamicScope_clear( DynamicScope* self )
{
    Py_CLEAR( self->owner );
    Py_CLEAR( self->change );
    Py_CLEAR( self->tracer );
    Py_CLEAR( self->f_locals );
    Py_CLEAR( self->f_globals );
    Py_CLEAR( self->f_builtins );
    Py_CLEAR( self->f_writes );
    Py_CLEAR( self->f_nonlocals );
}


int
DynamicScope_traverse( DynamicScope* self, visitproc visit, void* arg )
{
    Py_VISIT( self->owner );
    Py_VISIT( self->change );
    Py_VISIT( self->tracer );
    Py_VISIT( self->f_locals );
    Py_VISIT( self->f_globals );
    Py_VISIT( self->f_builtins );
    Py_VISIT( self->f_writes );
    Py_VISIT( self->f_nonlocals );
    Py_VISIT(Py_TYPE(self));
    return 0;
}


void
DynamicScope_dealloc( DynamicScope* self )
{
    PyTypeObject *tp = Py_TYPE(self);
    PyObject_GC_UnTrack( self );
    DynamicScope_clear( self );
    tp->tp_free( pyobject_cast( self ) );
    Py_DECREF(tp);
}


PyObject*
DynamicScope_getitem( DynamicScope* self, PyObject* key )
{
    if( !PyUnicode_CheckExact( key ) )
        return cppy::type_error( key, "str" );

    PyObject* res;

    // value from the override scope
    if( self->f_writes )
    {
        res = PyDict_GetItem( self->f_writes, key );
        if( res )
            return cppy::incref( res );
    }

    const char* key_data = PyUnicode_AsUTF8(key);
    if ( !key_data )
        return 0;

    // 'self' magic
    if( strcmp( key_data, "self" ) == 0 )
        return cppy::incref( self->owner );

    // 'change' magic
    if( self->change && strcmp( key_data, "change" ) == 0 )
        return cppy::incref( self->change );

    // 'nonlocals' magic
    if( strcmp( key_data, "nonlocals" ) == 0 )
    {
        if( !self->f_nonlocals )
        {
            self->f_nonlocals = PyType_GenericNew( Nonlocals::TypeObject, 0, 0 );
            if( !self->f_nonlocals )
                return 0;
            Nonlocals* nl = reinterpret_cast<Nonlocals*>( self->f_nonlocals );
            nl->owner = cppy::incref( self->owner );
            nl->tracer = cppy::xincref( self->tracer );
        }
        return cppy::incref( self->f_nonlocals );
    }

    // __scope__ magic
    if( strcmp( key_data, "__scope__" ) == 0 )
        return cppy::incref( pyobject_cast( self ) );

    // _[tracer] magic
    if( self->tracer && strcmp( key_data, "_[tracer]" ) == 0 )
        return cppy::incref( pyobject_cast( self->tracer ) );

    // value from the local scope
    res = PyObject_GetItem( self->f_locals, key );
    if( res )
        return res;
    if( PyErr_Occurred() )
    {
        if( !PyErr_ExceptionMatches( PyExc_KeyError ) )
            return 0;
        PyErr_Clear();
    }

    // super magic
    if( strcmp( key_data, "super" ) == 0 )
        return cppy::incref(super_disallowed);

    // value from the global scope
    res = PyDict_GetItem( self->f_globals, key );
    if( res )
        return cppy::incref( res );

    // value from the builtin scope
    if ( self-> f_builtins )
    {
        res = PyDict_GetItem( self->f_builtins, key );
        if( res )
            return cppy::incref( res );
    }

    res = load_dynamic_attr( self->owner, key, self->tracer );
    if( res )
        return res;
    if( PyErr_Occurred() )
        return 0;

    PyErr_SetObject( PyExc_KeyError, key );
    return 0;
}

PyObject*
DynamicScope_get( DynamicScope* self, PyObject*const *args, Py_ssize_t n )
{
    if( n < 1 || n > 2 )
        return cppy::type_error( "signature is get(key, default=None)" );
    PyObject* key = args[0];
    PyObject* res = DynamicScope_getitem(self, key);
    if ( res )
    {
        return res; // Ref already incremented
    }

    if( PyErr_Occurred() )
    {
        if( !PyErr_ExceptionMatches( PyExc_KeyError ) )
        {
            return 0;
        }
        PyErr_Clear();
    }

    return cppy::incref( n == 2 ? args[1] : Py_None );
}


int
DynamicScope_setitem( DynamicScope* self, PyObject* key, PyObject* value )
{
    if( !PyUnicode_CheckExact( key ) )
    {
        cppy::type_error( key, "str" );
        return -1;
    }
    if( !value )
    {
        if( self->f_writes )
            return PyDict_DelItem( self->f_writes, key );
        PyErr_SetObject( PyExc_KeyError, key );
        return -1;
    }
    if( !self->f_writes )
    {
        self->f_writes = PyDict_New();
        if( !self->f_writes )
            return -1;
    }
    return PyDict_SetItem( self->f_writes, key, value );
}


int
DynamicScope_contains( DynamicScope* self, PyObject* key )
{
    if( !PyUnicode_CheckExact( key ) )
    {
        cppy::type_error( key, "str" );
        return -1;
    }
    // value from the override scope
    if( self->f_writes && PyDict_GetItem( self->f_writes, key ) )
        return 1;

    const char* key_data = PyUnicode_AsUTF8(key);
    if ( !key_data )
        return 0;

    // 'self' magic
    if( strcmp( key_data, "self") == 0 )
        return 1;

    // 'change' magic
    if( self->change && strcmp( key_data, "change" ) == 0 )
        return 1;

    // 'nonlocals' magic
    if( strcmp( key_data, "nonlocals" ) == 0 )
        return 1;

    // __scope__ magic
    if( strcmp( key_data, "__scope__" ) == 0 )
        return 1;

    // super magic
    if( strcmp( key_data, "super" ) == 0 )
        return 1;

    // _[tracer] magic
    if( self->tracer && strcmp( key_data, "_[tracer]" ) == 0 )
        return 1;

    // value from the local scope
    cppy::ptr item( PyObject_GetItem( self->f_locals, key ) );
    if( item )
        return 1;
    if( PyErr_Occurred() )
    {
        if( !PyErr_ExceptionMatches( PyExc_KeyError ) )
            return -1;
        PyErr_Clear();
    }

    // value from the global scope
    if( PyDict_GetItem( self->f_globals, key ) )
        return 1;

    // value from the builtin scope
    if( self->f_builtins && PyDict_GetItem( self->f_builtins, key ) )
        return 1;

    return test_dynamic_attr( self->owner, key );
}

PyObject* DynamicScope_eval( DynamicScope* self, PyObject*const *args, Py_ssize_t nargsf)
{
    const auto n = PyVectorcall_NARGS(nargsf);
    if( n < 1 || n > 3 )
    {
        PyErr_SetString( PyExc_TypeError, "signature is eval(func, args[, kwargs])" );
        return 0;
    }
    PyObject* func = args[0];
    PyObject* func_args = n > 1 ? args[1] : 0;
    PyObject* func_kwargs = n > 2 ? args[2] : 0;
    if( !PyFunction_Check( func ) )
    {
        PyErr_SetString( PyExc_TypeError, "function must be a Python function" );
        return 0;
    }

    if( func_args && !PyTuple_Check( func_args ) )
    {
        PyErr_SetString( PyExc_TypeError, "arguments must be a tuple" );
        return 0;
    }

    if( func_kwargs && !PyDict_Check( func_kwargs ) )
    {
        PyErr_SetString( PyExc_TypeError, "keywords must be a dict" );
        return 0;
    }

    PyObject** arguments = 0;
    Py_ssize_t num_args = 0;
    if (func_args)
    {
        arguments = &PyTuple_GET_ITEM( func_args, 0 );
        num_args = PyTuple_GET_SIZE( func_args );
    }

    PyObject** defaults = 0;
    Py_ssize_t num_defaults = 0;
    PyObject* argdefs = PyFunction_GET_DEFAULTS( func );
    if( ( argdefs ) && PyTuple_Check( argdefs ) )
    {
        defaults = &PyTuple_GET_ITEM( reinterpret_cast<PyTupleObject*>( argdefs ), 0 );
        num_defaults = PyTuple_GET_SIZE( argdefs );
    }

    PyObject** keywords = 0;
    Py_ssize_t num_keywords = func_kwargs ? PyDict_GET_SIZE( func_kwargs ) : 0;
    if( num_keywords > 0 )
    {
        keywords = PyMem_NEW( PyObject*, 2 * num_keywords );
        if( !keywords )
            return PyErr_NoMemory();
        Py_ssize_t i = 0;
        Py_ssize_t pos = 0;
        while( PyDict_Next( func_kwargs, &pos, &keywords[ i ], &keywords[ i + 1 ] ) )
            i += 2;
        num_keywords = i / 2;
        /* XXX This is broken if the caller deletes dict items! */
    }

    PyObject* result = PyEval_EvalCodeEx(
        PyFunction_GET_CODE( func ),
        PyFunction_GET_GLOBALS( func ),
        pyobject_cast(self),
        arguments,
        num_args,
        keywords, num_keywords, defaults, num_defaults,
        NULL, PyFunction_GET_CLOSURE( func )
    );

    if( keywords )
        PyMem_DEL( keywords );

    return result;

}


PyObject* DynamicScope_get_owner( DynamicScope* self )
{
    return cppy::incref( self->owner );
}


PyObject* DynamicScope_get_change( DynamicScope* self )
{
    return cppy::incref( self->change ? self->change : Py_None );
}


PyObject* DynamicScope_get_f_locals( DynamicScope* self )
{
    return cppy::incref( self->f_locals );
}


PyObject* DynamicScope_get_f_globals( DynamicScope* self )
{
    return cppy::incref( self->f_globals );
}


PyObject* DynamicScope_get_f_builtins( DynamicScope* self )
{
    return cppy::incref( self->f_builtins ? self->f_builtins : Py_None );
}


PyObject* DynamicScope_get_f_writes( DynamicScope* self )
{
    return cppy::incref( self->f_writes ? self->f_writes : Py_None );
}

static PyGetSetDef
DynamicScope_getset[] = {
    { "_owner", ( getter )DynamicScope_get_owner, 0, "Get owner." },
    { "_change", ( getter )DynamicScope_get_change, 0, "Get change." },
    { "_f_locals", ( getter )DynamicScope_get_f_locals, 0, "Get f_locals." },
    { "_f_globals", ( getter )DynamicScope_get_f_globals, 0, "Get f_globals." },
    { "_f_builtins", ( getter )DynamicScope_get_f_builtins, 0, "Get f_builtins." },
    { "_f_writes", ( getter )DynamicScope_get_f_writes, 0, "Get f_writes." },
    { 0 } // sentinel
};


static PyMethodDef DynamicScope_methods[] = {
    {"get",    reinterpret_cast<PyCFunction>(DynamicScope_get), METH_FASTCALL, ""},
    {"eval",    reinterpret_cast<PyCFunction>(DynamicScope_eval), METH_FASTCALL, "Evaluate a function in the scope"},
    { 0 }  // Sentinel
};

static PyType_Slot DynamicScope_Type_slots[] = {
    { Py_tp_dealloc, void_cast( DynamicScope_dealloc ) },           /* tp_dealloc */
    { Py_tp_traverse, void_cast( DynamicScope_traverse ) },         /* tp_traverse */
    { Py_tp_clear, void_cast( DynamicScope_clear ) },               /* tp_clear */
    { Py_tp_new, void_cast( DynamicScope_new ) },                /* tp_new */
    { Py_tp_alloc, void_cast( PyType_GenericAlloc ) },           /* tp_alloc */
    { Py_tp_free, void_cast( PyObject_GC_Del ) },                /* tp_free */
    { Py_tp_getset, void_cast( DynamicScope_getset ) },          /* tp_getset */
    { Py_tp_methods, void_cast( DynamicScope_methods ) },        /* tp_methods */
    { Py_mp_subscript, void_cast( DynamicScope_getitem ) },      /* mp_subscript */
    { Py_mp_ass_subscript, void_cast( DynamicScope_setitem ) },  /* mp_ass_subscript */
    { Py_sq_contains, void_cast( DynamicScope_contains ) },      /* sq_contains */
//#if defined(Py_tp_vectorcall)
//    { Py_tp_vectorcall, void_cast( DynamicScope_vectorcall ) },      /* tp_vectorcall */
//#endif
    { 0, 0 },
};

}  // namespace


// Initialize static variables (otherwise the compiler eliminates them)
PyTypeObject* DynamicScope::TypeObject = NULL;


PyType_Spec DynamicScope::TypeObject_Spec = {
	"enaml._dynamicscope._DynamicScope",    /* tp_name */
	sizeof( DynamicScope ),               /* tp_basicsize */
	0,                                    /* tp_itemsize */
	Py_TPFLAGS_DEFAULT
	|Py_TPFLAGS_BASETYPE
    |Py_TPFLAGS_HAVE_GC
    |Py_TPFLAGS_DICT_SUBCLASS,            /* tp_flags */
    DynamicScope_Type_slots               /* slots */
};


bool DynamicScope::Ready()
{
    // The reference will be handled by the module to which we will add the type
	TypeObject = pytype_cast( PyType_FromSpec( &TypeObject_Spec ) );
    if( !TypeObject )
    {
        return false;  // LCOV_EXCL_LINE (failed to create type)
    }
    return true;
}


// Module definition
namespace
{


int
dynamicscope_modexec( PyObject *mod )
{
    parent_str = PyUnicode_InternFromString( "_parent" );
    if( !parent_str )
    {
        return -1;  // LCOV_EXCL_LINE (failed to create string)
    }
    dynamic_load_str = PyUnicode_InternFromString( "dynamic_load" );
    if( !dynamic_load_str )
    {
        return -1;  // LCOV_EXCL_LINE (failed to create string)
    }

    d_storage_str = PyUnicode_InternFromString("_d_storage");
    if ( !d_storage_str )
        return -1;  // LCOV_EXCL_LINE (failed to create string)
    get_str = PyUnicode_InternFromString("get");
    if ( !get_str )
        return -1;  // LCOV_EXCL_LINE (failed to create string)


    UserKeyError = PyErr_NewException( "dynamicscope.UserKeyError", 0, 0 );
    if( !UserKeyError )
    {
        return -1;  // LCOV_EXCL_LINE (failed to create type)
    }

    if( !Nonlocals::Ready() )
    {
        return -1;  // LCOV_EXCL_LINE (failed to create type)
    }
    if( !DynamicScope::Ready() )
    {
        return -1;  // LCOV_EXCL_LINE (failed to create type)
    }

    // DynamicScope
    cppy::ptr dynamicscope( pyobject_cast( DynamicScope::TypeObject ) );
	if( PyModule_AddObject( mod, "_DynamicScope", dynamicscope.get() ) < 0 )
	{
		return -1;  // LCOV_EXCL_LINE (failed to add to module)
	}
    dynamicscope.release();


    if( PyModule_AddObjectRef( mod, "UserKeyError", UserKeyError ) < 0 )
        return -1; // LCOV_EXCL_LINE (failed to add to module)

    super_disallowed = PyObject_GetAttrString( mod, "_super_disallowed" );
    if( !super_disallowed )
        return -1;  // LCOV_EXCL_LINE (failed import of known existing function)

    return 0;
}


static PyMethodDef
dynamicscope_methods[] = {
    {"_super_disallowed", ( PyCFunction )_SuperDisallowed,
        METH_VARARGS | METH_KEYWORDS, "Forbid use of super in declarative function"},
    { 0 }  // Sentinel
};


PyModuleDef_Slot dynamicscope_slots[] = {
    {Py_mod_exec, reinterpret_cast<void*>( dynamicscope_modexec ) },
    {0, NULL}
};


struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_dynamicscope",
        "dynamicscope extension module",
        0,
        dynamicscope_methods,
        dynamicscope_slots,
        NULL,
        NULL,
        NULL
};


}  // namespace


}  // namespace enaml


PyMODINIT_FUNC PyInit__dynamicscope( void )
{
    return PyModuleDef_Init( &enaml::moduledef );
}
