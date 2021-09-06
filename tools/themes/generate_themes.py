#------------------------------------------------------------------------------
# Copyright (c) 2021, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file LICENSE, distributed with this software.
#------------------------------------------------------------------------------
import re
import os
import sys
import json
import argparse
from typing import Dict
from glob import glob
from datetime import datetime

# Header
HEADER ="""\
#-----------------------------------------------------------------------------
# Copyright (c) 2017-%s, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file LICENSE, distributed with this software.
#-----------------------------------------------------------------------------

""" % datetime.now().year

# Map pygments token name to scintilla
TOKEN_MAPPING = [
    ("name_class", "class_name"),
    ("comment", "comment"),
    ("comment_multiline", "comment_bock"),
    ("name_decorator", "decorator"),
    ("default", "default"),
    ("literal_string", "single_quoted_string"),
    ("literal_string", "double_quoted_string"),
    ("name_function", "function_method_name"),
    ("name_variable", "highlighted_identifier"),
    ("name_variable", "identifier"),
    ("keyword", "keyword"),
    ("literal_number", "number"),
    ("operator_word", "operator"),
    ("literal_string", "single_quoted_string"),
    ("literal_string", "triple_double_quoted_string"),
    ("literal_string", "triple_single_quoted_string"),
    ("error", "unclosed_string"),
]

# CSS line pattern
PATTERN = r"\.highlight (?:\.([a-z0-9]+))?\s*{ (.+) }\s*(?:/\* (.+) \*/)?"


def parse(css_file: str) -> Dict[str, Dict[str, str]]:
    """ Parse the css file and return the mapping of comment to style dict.

    Parameters
    ----------
    css_file: str
        The file path to parse

    Returns
    -------
    styles: dict
        The parsed styles. The comment is lowered and '.' replaced with '_'

    """
    items = {}
    with open(css_file) as f:
        data = f.read()
    for line in data.split("\n"):
        if not line:
            continue
        m = re.match(PATTERN, line)
        if not m:
            continue
        key, styles, label = m.groups()

        # If label/comment is missing use the set it to default or the css key
        if label is None and key is None:
            label = "default"
        elif label is None:
            label = "background" if key == "hll" else key

        style = {}
        for item in styles.split(";"):
            if item:
                k, v = item.split(":")
                style[k.strip()] = v.strip()
        items[label.lower().replace(".", "_")] = style
    return items

def generate(css_file: str, output_dir: str):
    """ Generate a theme for the given css file path.

    Parameters
    ----------
    css_file: str
        The css file to parse
    output_dir: str
        The output path to save the file in
    """
    print(css_file)
    css = parse(css_file)
    theme_name = os.path.basename(css_file)[:-4]

    theme = {}

    background = "#ffffff"
    color = "#000000"
    if "default" in css:
        defaults = css["default"]
        background = defaults.get("background", background)
        color = defaults.get("color", color)

    if "hll" in css:
        background = background.get("background-color", background)

    settings = {
        "name": theme_name,
        "paper": background,
        "caret": color,
        "color": color,
    }

    for pygments_name, styles in css.items():
        for name, token_name in TOKEN_MAPPING:
            if pygments_name == name:
                theme[token_name] = styles

    class_name = f'{theme_name.upper()}_THEME'
    output = {
        "python": theme,
        "settings": settings,
    }

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    with open(f"{output_dir}/{theme_name}.py", "w") as f:
        f.write(HEADER)
        f.write("%s = " % class_name)
        f.write(json.dumps(output, indent=4, sort_keys=True))
        f.write("\n\n")
        f.write("%s['enaml'] = %s['python']\n" % (class_name, class_name))


def main():
    parser = argparse.ArgumentParser(
        description='Generate themes from pygments themes.')
    parser.add_argument(
        '-i', '--input_dir', type=str, default="./",
        required=False, dest='input_dir',
        help="directory to search for pygments css files")
    parser.add_argument(
        '-o', '--output_dir', type=str, default="./output",
        required=False, dest='output_dir',
        help="directory to save generated themes")
    args = parser.parse_args()
    css_files = glob(f"{args.input_dir}*/*.css")
    if not css_files:
        print("no css files found")
        sys.exit(1)

    for css_file in sorted(css_files):
        generate(css_file, args.output_dir)


if __name__ == '__main__':
    main()
