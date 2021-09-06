#-----------------------------------------------------------------------------
# Copyright (c) 2017-2021, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file LICENSE, distributed with this software.
#-----------------------------------------------------------------------------

BW_THEME = {
    "python": {
        "class_name": {
            "font-weight": "bold"
        },
        "comment": {
            "font-style": "italic"
        },
        "comment_block": {
            "font-style": "italic"
        },
        "default": {
            "background": "#ffffff"
        },
        "double_quoted_string": {
            "font-style": "italic"
        },
        "keyword": {
            "font-weight": "bold"
        },
        "operator": {
            "font-weight": "bold"
        },
        "single_quoted_string": {
            "font-style": "italic"
        },
        "triple_double_quoted_string": {
            "font-style": "italic"
        },
        "triple_single_quoted_string": {
            "font-style": "italic"
        },
        "unclosed_string": {
            "border": "1px solid #FF0000"
        }
    },
    "settings": {
        "caret": "#000000",
        "color": "#000000",
        "name": "bw",
        "paper": "#ffffff"
    }
}

BW_THEME['enaml'] = BW_THEME['python']
