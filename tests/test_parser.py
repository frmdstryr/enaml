#------------------------------------------------------------------------------
# Copyright (c) 2013-2018, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file COPYING.txt, distributed with this software.
#------------------------------------------------------------------------------
from textwrap import dedent
from utils import compile_source


def test_vargslist18():
    """Test running a list comprehension in an operator handler.

    """
    source = dedent("""
    def foobar(first=1, second=2, **kwargs):
        pass
    """)
    compile_source(source, 'foobar')
