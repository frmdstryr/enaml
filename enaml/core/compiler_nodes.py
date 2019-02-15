#------------------------------------------------------------------------------
# Copyright (c) 2013, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file COPYING.txt, distributed with this software.
#------------------------------------------------------------------------------
from contextlib import contextmanager

from atom.api import Atom, Bool, Str, Tuple, Typed, ForwardTyped
from atom.datastructures.api import sortedmap

from .expression_engine import ExpressionEngine


#: The private stack of active local scopes.
__stack = []


#: The private map of active local scopes.
__map = {}


@contextmanager
def new_scope(key, seed=None):
    """ Create a new scope mapping and push it onto the stack.

    The currently active scope can be retrieved with 'peek_scope' and
    a specific scope can be retrieved with 'fetch_scope'.

    Parameters
    ----------
    key : object
        The scope key to associate with this local scope.

    seed : sortedmap, optional
        The seed map values for creating the scope.

    Returns
    -------
    result : contextmanager
        A contextmanager which will pop the scope after the context
        exits. It yields the new scope as the context variable.

    """
    if seed is not None:
        scope = seed.copy()
    else:
        scope = sortedmap()
    __map[key] = scope
    __stack.append(scope)
    yield scope
    __stack.pop()
    del __map[key]


def peek_scope():
    """ Get the local scope object from the top of the scope stack.

    Returns
    -------
    result : sortedmap
        The active scope mapping.

    """
    return __stack[-1]


def fetch_scope(key):
    """ Fetch a specific local scope by key.

    Parameters
    ----------
    key : object
        The scope key associated with the scope of interest.

    Returns
    -------
    result : sortedmap
        The relevant local scope.

    """
    return __map[key]


class SuperProxy(Atom):
    owner = Typed(Atom)
    base = Typed(object)

    def __getattr__(self, attr):
        owner = self.owner
        return owner._d_engine.read(owner, attr, self.base)


class CompilerNode(Atom):
    """ A base class for defining compiler nodes.

    """
    #: The scope key for the for the local scope of the node.
    scope_key = Typed(object)

    #: The child compiler nodes of this node.
    children = Typed(list, ())

    #: A mapping of id->node for the nodes in the block. This mapping
    #: is shared among all nodes in the same block.
    id_nodes = Typed(sortedmap)

    def update_id_nodes(self, mapping):
        """ Recursively update the id nodes for this node.

        Parameters
        ----------
        mapping : sortedmap
            The mapping to fill with the identifier information.

        """
        self.id_nodes = mapping
        for child in self.children:
            child.update_id_nodes(mapping)

    def copy(self):
        """ Create a copy of the compiler node.

        """
        node = type(self)()
        node.scope_key = self.scope_key
        node.children = [child.copy() for child in self.children]
        return node


class DeclarativeNode(CompilerNode):
    """ A compiler node which represents a declarative declaration.

    Instances of this class are generated by the compiler and contain
    the information needed to create an instance of the hierarchy at
    runtime.

    """
    #: The declarative type object to instantiate.
    klass = Typed(type)

    #: The local identifier to associate with the instance.
    identifier = Str()

    #: Whether or not the node should store the locals in the map.
    store_locals = Bool(False)

    #: Whether or not the instance intercepts the child nodes.
    child_intercept = Bool(False)

    #: The expression engine to associate with the instance.
    engine = Typed(ExpressionEngine)

    #: The set of scope keys for the closure scopes. This will be None
    #: if the node does not require any closure scopes.
    closure_keys = Typed(set)

    #: The superclass nodes of this node. This will be None if the
    #: node represents a raw declarative object vs an enamldef.
    super_node = ForwardTyped(lambda: EnamlDefNode)

    def __call__(self, parent):
        """ Instantiate the type hierarchy.

        This is invoked by a parent compiler node when the declarative
        hierarchy is being instantiated.

        Parameters
        ----------
        parent : Declarative or None
            The parent declarative object for the hierarchy.

        Returns
        -------
        result : Declarative
            The declarative instance created by the node.

        """
        klass = self.klass
        instance = klass.__new__(klass)
        self.populate(instance)
        instance.__init__(parent)
        return instance

    def populate(self, instance):
        """ Populate an instance generated for the node.

        Parameters
        ----------
        instance : Declarative
            The declarative instance for this node.

        """
        f_locals = peek_scope()
        if self.super_node is not None:
            self.super_node(instance)
            base = self.super_node.klass
            f_locals['super'] = SuperProxy(owner=instance, base=base)

        scope_key = self.scope_key
        if self.identifier:
            f_locals[self.identifier] = instance
        if self.store_locals:
            instance._d_storage[scope_key] = f_locals
        if self.engine is not None:
            instance._d_engine = self.engine
        if self.closure_keys is not None:
            for key in self.closure_keys:
                instance._d_storage[key] = fetch_scope(key)
        if self.child_intercept:
            children_copy = self.children[:]
            instance.child_node_intercept(children_copy, scope_key, f_locals)
        else:
            for node in self.children:
                node(instance)

    def size(self):
        """ Return the size of the instantiated node.

        """
        return 1

    def update_id_nodes(self, mapping):
        """ Update the id nodes for this node.

        Parameters
        ----------
        mapping : sortedmap
            The mapping to fill with the identifier information.

        """
        if self.identifier:
            mapping[self.identifier] = self
        super(DeclarativeNode, self).update_id_nodes(mapping)

    def copy(self):
        """ Create a copy of this portion of the node hierarchy.

        Returns
        -------
        result : DeclarativeNode
            A copy of the node hierarchy from this node down.

        """
        node = super(DeclarativeNode, self).copy()
        node.klass = self.klass
        node.identifier = self.identifier
        node.store_locals = self.store_locals
        node.child_intercept = self.child_intercept
        if self.engine is not None:
            node.engine = self.engine.copy()
        if self.super_node is not None:
            node.super_node = self.super_node.copy()
        if self.closure_keys is not None:
            node.closure_keys = self.closure_keys.copy()
        return node


class EnamlDefNode(DeclarativeNode):
    """ A declarative node which represents an 'enamldef' block.

    """
    def __call__(self, instance):
        """ Instantiate the declarative hierarchy for the node.

        This is invoked by the EnamlDefMeta class when an enamldef
        class is called, or when a DeclarativeNode invokes its
        super node.

        Parameters
        ----------
        instance : EnamlDef
            The enamldef instance which should be populated.

        """
        with new_scope(self.scope_key):
            self.populate(instance)

    def update_id_nodes(self):
        """ Update the id nodes for this node.

        """
        mapping = sortedmap()
        if self.identifier:
            mapping[self.identifier] = self
        super(DeclarativeNode, self).update_id_nodes(mapping)

    def copy(self):
        """ Create a copy the enamldef node hierarchy.

        """
        node = super(EnamlDefNode, self).copy()
        node.update_id_nodes()
        return node


class TemplateNode(CompilerNode):
    """ A compiler node which represents a template declaration.

    """
    #: The params and consts for the template instantiation. This is
    #: provided by the compiler, and should be considered read-only.
    scope = Typed(sortedmap, ())

    def __call__(self, parent):
        """ Instantiate the type hierarchy.

        Parameters
        ----------
        parent : Declarative or None
            The parent declarative object for the templates.

        Returns
        -------
        result : list
            The list of declarative objects generated by the template.

        """
        instances = []
        with new_scope(self.scope_key, self.scope):
            for node in self.children:
                if isinstance(node, DeclarativeNode):
                    instances.append(node(parent))
                elif isinstance(node, TemplateInstanceNode):
                    instances.extend(node(parent))
        return instances

    def update_id_nodes(self):
        """ Update the id nodes for this node.

        """
        super(TemplateNode, self).update_id_nodes(sortedmap())

    def size(self):
        """ Return the size of the instantiated node.

        """
        return sum(child.size() for child in self.children)

    def iternodes(self):
        """ Iterate over the nodes of the template.

        Returns
        -------
        result : generator
            A generator which yields the unrolled nodes of the template
            instantiation.

        """
        for child in self.children:
            if isinstance(child, DeclarativeNode):
                yield child
            elif isinstance(child, TemplateInstanceNode):
                for node in child.iternodes():
                    yield node

    def copy(self):
        """ Create a copy of the node.

        """
        node = super(TemplateNode, self).copy()
        node.scope = self.scope
        node.update_id_nodes()
        return node


class TemplateInstanceNode(CompilerNode):
    """ A compiler node which represents a template instantiation.

    """
    #: The template node which is invoked to generate the object.
    template = Typed(TemplateNode)

    #: The named identifiers for the instantiated objects.
    names = Tuple()

    #: The starname identifier for the instantiated objects.
    starname = Str()

    def __call__(self, parent):
        """ Invoke the template instantiation to build the objects.

        Parameters
        ----------
        parent : Declarative
            The parent declarative object for the instantiation.

        """
        instances = self.template(parent)
        f_locals = peek_scope()
        if self.names:
            for name, instance in zip(self.names, instances):
                f_locals[name] = instance
        if self.starname:
            f_locals[self.starname] = tuple(instances[len(self.names):])
        return instances

    def update_id_nodes(self, mapping):
        """ Update the id nodes for this node.

        Parameters
        ----------
        mapping : sortedmap
            The mapping to fill with the identifier information.

        """
        if self.names:
            nodeiter = self.iternodes()
            for name in self.names:
                mapping[name] = next(nodeiter)
        super(TemplateInstanceNode, self).update_id_nodes(mapping)

    def size(self):
        """ Return the size of the instantiated node.

        """
        return self.template.size()

    def iternodes(self):
        """ Iterate over the nodes of the instantiation.

        Returns
        -------
        result : generator
            A generator which yields the unrolled nodes of the template
            instantiation.

        """
        return self.template.iternodes()

    def copy(self):
        """ Create a copy of the node.

        """
        node = super(TemplateInstanceNode, self).copy()
        node.template = self.template.copy()
        node.names = self.names
        node.starname = self.starname
        return node
