#------------------------------------------------------------------------------
# Copyright (c) 2017, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file COPYING.txt, distributed with this software.
#------------------------------------------------------------------------------
from atom.api import ForwardInstance, Enum

from .declarative import Declarative, d_


class Block(Declarative):
    """ An object which allows its children to be dynamically replaced by the children of another block.

    This allows you to make extendible components where certain pieces can be overwritten.  When a
    'Block' reference is assigned to the 'block' property, the blocks children will either replace  
    the referenced block's children with this blocks children, or append it's children to the referenced
    block's children.

    Creating a 'Block' with no parent is a programming error.
    
    """

    #: The Block to which this blocks children should be inserted into 
    block = d_(ForwardInstance(lambda: Block))

    #: If replace, replace all parent's children (except the block of course) 
    mode = d_(Enum('replace', 'append'))

    def initialize(self):
        """ A reimplmeneted initializer.

        This method will add the include objects to the parent of the
        include and ensure that they are initialized.

        """
        super(Block, self).initialize()

        if self.block:  #: This block is setting the content of another block
            #: Remove the existing blocks children
            if self.mode == 'replace':
                #: Clear the blocks children
                for c in self.block.children:
                    c.destroy()
            #: Add this blocks children to the other block
            self.block.insert_children(None, self.children)
        else:  #: This block is inserting it's children into it's parent
            self.parent.insert_children(self, self.children)

    def _observe_block(self, change):
        """ A change handler for the 'objects' list of the Include.

        If the object is initialized objects which are removed will be
        unparented and objects which are added will be reparented. Old
        objects will be destroyed if the 'destroy_old' flag is True.

        """
        if self.is_initialized:
            if change['type'] == 'update':
                raise NotImplementedError
                old_block = change['oldvalue']
                old_block.parent.remove_children(old_block, self.children)
                new_block = change['value']
                new_block.parent.insert_children(new_block, self.children)

    def _observe__children(self, change):
        if not self.is_initialized:
            return
        if change['type'] == 'update':
            if self.block:
                if self.mode == 'replace':
                    self.block.children = change['value']
                else:
                    for c in change['oldvalue']:
                        self.block.children.remove(c)
                        c.destroy()
                    before = self.block.children[-1] if self.block.children else None
                    self.block.insert_children(before, change['value'])
            else:
                for c in change['oldvalue']:
                    if c not in change['value']:
                        c.destroy()
                self.parent.insert_children(self, change['value']) 
