#------------------------------------------------------------------------------
# Copyright (c) 2013, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file LICENSE, distributed with this software.
#------------------------------------------------------------------------------
from abc import ABCMeta
from collections.abc import Iterable, Iterator

from atom.api import Atom, Int, Coerced, List, Typed, Value
from atom.datastructures.api import sortedmap

from .compiler_nodes import new_scope
from .declarative import d_
from .pattern import Pattern


def coerce_iterable(iterable):
    """ Coerce iterators to a tuple or return the iterable as is.

    """
    if isinstance(iterable, Iterator):
        return tuple(iterable)
    elif isinstance(iterable, Iterable):
        return iterable
    raise TypeError("%s object is not iterable" % type(iterable))


class LooperIterableMeta(ABCMeta):
    """ Metaclass which checks if an instance is Iterable but not an Iterator.

    """
    def __instancecheck__(self, instance):
        if isinstance(instance, Iterator):
            return False
        return isinstance(instance, Iterable)


class LooperIterable(Iterable, metaclass=LooperIterableMeta):
    """ An Iterable that is not an Iterator

    """


class Iteration(Atom):
    """ A container to hold data for items in the Looper.

    """
    #: Index within the Looper's iterable
    index = Int()

    #: Item from the Looper's iterable
    item = Value()

    #: Nodes generated by the Looper
    nodes = List()


class Looper(Pattern):
    """ A pattern object that repeats its children over an iterable.

    The children of a `Looper` are used as a template when creating new
    objects for each item in the given `iterable`. Each iteration of the
    loop will be given an independent scope which is the union of the
    outer scope and any identifiers created during the iteration. This
    scope will also contain a `loop` variable which has `item` and `index`
    members to access the index and value of the iterable, respectively.

    All items created by the looper will be added as children of the
    parent of the `Looper`. The `Looper` keeps ownership of all items
    it creates. When the iterable for the looper is changed, the looper
    will only create and destroy children for the items in the iterable
    which have changed. When an item in the iterable is moved the
    `loop.index` will be updated to reflect the new index.

    The Looper works under the assumption that the values stored in the
    iterable are unique.

    The `loop_item` and `loop_index` scope variables are depreciated in favor
    of `loop.item` and `loop.index` respectively. This is because the old
    `loop_index` variable may become invalid when items are moved.

    """
    #: The iterable to use when creating the items for the looper.
    #: The items in the iterable must be unique. This allows the
    #: Looper to optimize the creation and destruction of widgets.
    #: If the iterable is an Iterator it is first coerced to a tuple.
    iterable = d_(Coerced(LooperIterable, coercer=coerce_iterable))

    #: The list of items created by the conditional. Each item in the
    #: list represents one iteration of the loop and is a list of the
    #: items generated during that iteration. This list should not be
    #: manipulated directly by user code.
    items = List()

    #: Private data storage which maps the user iterable data to the
    #: list of items created for that iteration. This allows the looper
    #: to only create and destroy the items which have changed.
    _iter_data = Typed(sortedmap, ())

    #--------------------------------------------------------------------------
    # Lifetime API
    #--------------------------------------------------------------------------
    def destroy(self):
        """ A reimplemented destructor.

        The looper will release the owned items on destruction.

        """
        super(Looper, self).destroy()
        del self.iterable
        del self.items
        del self._iter_data

    #--------------------------------------------------------------------------
    # Observers
    #--------------------------------------------------------------------------
    def _observe_iterable(self, change):
        """ A private observer for the `iterable` attribute.

        If the iterable changes while the looper is active, the loop
        items will be refreshed.

        """
        if change['type'] == 'update' and self.is_initialized:
            self.refresh_items()

    #--------------------------------------------------------------------------
    # Pattern API
    #--------------------------------------------------------------------------
    def pattern_items(self):
        """ Get a list of items created by the pattern.

        """
        return sum(self.items, [])

    def refresh_items(self):
        """ Refresh the items of the pattern.

        This method destroys the old items and creates and initializes
        the new items.

        """
        old_items = self.items[:]
        old_iter_data = self._iter_data
        iterable = self.iterable
        pattern_nodes = self.pattern_nodes
        new_iter_data = sortedmap()
        new_items = []

        if iterable is not None and len(pattern_nodes) > 0:
            for loop_index, loop_item in enumerate(iterable):
                iter_data = old_iter_data.get(loop_item)
                if iter_data is not None:
                    new_iter_data[loop_item] = iter_data
                    iteration = iter_data.nodes
                    new_items.append(iteration)
                    old_items.remove(iteration)
                    iter_data.index = loop_index
                    continue
                iter_data = Iteration(index=loop_index, item=loop_item)
                iteration = iter_data.nodes
                new_iter_data[loop_item] = iter_data
                new_items.append(iteration)
                for nodes, key, f_locals in pattern_nodes:
                    with new_scope(key, f_locals) as f_locals:
                        # Retain for compatibility reasons
                        f_locals['loop_index'] = loop_index
                        f_locals['loop_item'] = loop_item
                        f_locals['loop'] = iter_data
                        for node in nodes:
                            child = node(None)
                            if isinstance(child, list):
                                iteration.extend(child)
                            else:
                                iteration.append(child)

        for iteration in old_items:
            for old in iteration:
                if not old.is_destroyed:
                    old.destroy()

        if len(new_items) > 0:
            expanded = []
            recursive_expand(sum(new_items, []), expanded)
            self.parent.insert_children(self, expanded)

        self.items = new_items
        self._iter_data = new_iter_data


def recursive_expand(items, expanded):
    """ Recursively expand the list of items created by the looper.

    This allows the final list to be inserted into the parent and
    maintain the proper ordering of children.

    Parameters
    ----------
    items : list
        The list of items to expand. This should be composed of
        Pattern and other Object instances.

    expanded : list
        The output list. This list will be modified in-place.

    """
    for item in items:
        if isinstance(item, Pattern):
            recursive_expand(item.pattern_items(), expanded)
        expanded.append(item)
