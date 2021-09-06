#-----------------------------------------------------------------------------
# Copyright (c) 2017-2021, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file LICENSE, distributed with this software.
#-----------------------------------------------------------------------------

BORLAND_THEME = {
    "python": {
        "comment": {
            "color": "#008800",
            "font-style": "italic"
        },
        "comment_block": {
            "color": "#008800",
            "font-style": "italic"
        },
        "default": {
            "background": "#ffffff"
        },
        "double_quoted_string": {
            "color": "#0000FF"
        },
        "keyword": {
            "color": "#000080",
            "font-weight": "bold"
        },
        "number": {
            "color": "#0000FF"
        },
        "operator": {
            "font-weight": "bold"
        },
        "single_quoted_string": {
            "color": "#0000FF"
        },
        "triple_double_quoted_string": {
            "color": "#0000FF"
        },
        "triple_single_quoted_string": {
            "color": "#0000FF"
        },
        "unclosed_string": {
            "background-color": "#e3d2d2",
            "color": "#a61717"
        }
    },
    "settings": {
        "caret": "#000000",
        "color": "#000000",
        "name": "borland",
        "paper": "#ffffff"
    }
}

BORLAND_THEME['enaml'] = BORLAND_THEME['python']
