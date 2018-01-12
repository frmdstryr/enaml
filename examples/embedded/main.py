#------------------------------------------------------------------------------
# Copyright (c) 2013, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file COPYING.txt, distributed with this software.
#------------------------------------------------------------------------------
import os
import enaml
import argparse
import subprocess
from enaml.qt.qt_application import QtApplication


def main_embedded():
    with enaml.imports():
        from view import Embedded as Main

    app = QtApplication()
    view = Main(pid=os.getpid())
    view.show()

    #: Print the window ID after starting
    app.deferred_call(lambda: print(int(view.proxy.widget.winId())))
    app.start()


def main():

    with enaml.imports():
        from view import Main

    app = QtApplication()

    #: Spawn the child and read the window id
    process = subprocess.Popen(['python', 'main.py', '--embedded'],
                                stdout=subprocess.PIPE)
    window_id = int(process.stdout.readline().strip())

    view = Main(window_id=window_id, pid=os.getpid())
    view.show()

    # Start the application event loop
    app.start()

    #: Stop the embedded application
    process.kill()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--embedded", action="store_true",
                        help="Run the child view")
    args = parser.parse_args()
    if args.embedded:
        main_embedded()
    else:
        main()
