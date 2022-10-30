#------------------------------------------------------------------------------
# Copyright (c) 2013-2021, Nucleic Development Team.
#
# Distributed under the terms of the Modified BSD License.
#
# The full license is in the file LICENSE, distributed with this software.
#------------------------------------------------------------------------------
import os
import sys
from setuptools import find_packages, Extension, setup
from setuptools.command.build_ext import build_ext

sys.path.insert(0, os.path.abspath('.'))
from enaml.version import __version__

# Use the env var ENAML_DISABLE_FH4 to disable linking against VCRUNTIME140_1.dll

ext_modules = [
    Extension(
        'enaml.weakmethod',
        ['enaml/src/weakmethod.cpp'],
        language='c++',
    ),
    Extension(
        'enaml.callableref',
        ['enaml/src/callableref.cpp'],
        language='c++',
    ),
    Extension(
       'enaml.signaling',
       ['enaml/src/signaling.cpp'],
       language='c++',
    ),
    Extension(
        'enaml.core.funchelper',
        ['enaml/src/funchelper.cpp'],
        language='c++',
    ),
    Extension(
        'enaml.colorext',
        ['enaml/src/colorext.cpp'],
        language='c++',
    ),
    Extension(
        'enaml.fontext',
        ['enaml/src/fontext.cpp'],
        language='c++',
    ),
    Extension(
        'enaml.core.dynamicscope',
        ['enaml/src/dynamicscope.cpp'],
        language='c++',
    ),
    Extension(
        'enaml.core.alias',
        ['enaml/src/alias.cpp'],
        language='c++',
    ),
    Extension(
        'enaml.core.declarative_function',
        ['enaml/src/declarative_function.cpp'],
        language='c++',
    ),
]


if sys.platform == 'win32':
    ext_modules.append(
        Extension(
            'enaml.winutil',
            ['enaml/src/winutil.cpp'],
            libraries=['user32', 'gdi32'],
            language='c++'
        )
    )


class BuildExt(build_ext):
    """ A custom build extension for adding compiler-specific options.

    """
    c_opts = {
        'msvc': ['/EHsc']
    }

    def initialize_options(self):
        build_ext.initialize_options(self)
        self.debug = False

    def build_extensions(self):

        # Delayed import of cppy to let setup_requires install it if necessary
        import cppy

        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        for ext in self.extensions:
            ext.include_dirs.insert(0, cppy.get_include())
            ext.extra_compile_args = opts
            if sys.platform == 'darwin':
                ext.extra_compile_args += ['-stdlib=libc++']
                ext.extra_link_args += ['-stdlib=libc++']
            if (ct == 'msvc' and os.environ.get('ENAML_DISABLE_FH4')):
                # Disable FH4 Exception Handling implementation so that we don't
                # require VCRUNTIME140_1.dll. For more details, see:
                # https://devblogs.microsoft.com/cppblog/making-cpp-exception-handling-smaller-x64/
                # https://github.com/joerick/cibuildwheel/issues/423#issuecomment-677763904
                ext.extra_compile_args.append('/d2FH4-')
        build_ext.build_extensions(self)


setup(
    name='enaml',
    version=__version__,
    author='The Nucleic Development Team',
    author_email='sccolbert@gmail.com',
    url='https://github.com/nucleic/enaml',
    description='Declarative DSL for building rich user interfaces in Python',
    long_description=open('README.rst').read(),
    license='BSD',
    classifiers=[
          # https://pypi.org/pypi?%3Aaction=list_classifiers
          'License :: OSI Approved :: BSD License',
          'Programming Language :: Python',
          'Programming Language :: Python :: 3',
          'Programming Language :: Python :: 3.8',
          'Programming Language :: Python :: 3.9',
          'Programming Language :: Python :: 3.10',
          'Programming Language :: Python :: Implementation :: CPython',
      ],
    python_requires='>=3.8',
    requires=['atom', 'qtpy', 'pegen', 'kiwisolver'],
    install_requires=['atom>=0.7.0',
                      'kiwisolver>=1.2.0',
                      'pegen',
                      "bytecode>=0.11.0"
                      ],
    setup_requires=['cppy>=1.1.0'],
    extras_require={
        "qt5-pyqt": ["qtpy", "pyqt5"],
        "qt5-pyside": ["qtpy", "pyside2"]
    },
    packages=find_packages(),
    package_data={
        'enaml.core.parser': ["*.gram"],
        'enaml.applib': ['*.enaml'],
        'enaml.stdlib': ['*.enaml'],
        'enaml.workbench.core': ['*.enaml'],
        'enaml.workbench.ui': ['*.enaml'],
        'enaml.qt.docking': [
            'dock_images/*.png',
            'dock_images/*.py',
            'enaml_dock_resources.qrc'
        ],
    },
    entry_points={'console_scripts': [
        'enaml-run = enaml.runner:main',
        'enaml-compileall = enaml.compile_all:main',
    ]},
    ext_modules=ext_modules,
    cmdclass={'build_ext': BuildExt},
)
