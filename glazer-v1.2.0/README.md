
GLAZER
======

by Andrew Apted, 2025.


About
-----

Glazer is an assembler which targets the Glulx virtual machine.


Examples
--------

Hello world sample: [hello.asm](hello.asm)

Conway's game of life: [life.asm](life.asm)


Status
------

Current version is 1.2.0

There are no known bugs.


Downloads
---------

https://gitlab.com/andwj/glazer/-/releases/v1.2.0


Documentation
-------------

Please read the fine manual: [MANUAL.md](MANUAL.md)


Legalese
--------

Glazer is under a permissive MIT license.

Glazer comes with NO WARRANTY of any kind, express or implied.

See the [LICENSE.md](LICENSE.md) doc for the full terms.


Compiling
---------

Firstly, either clone the git repository, or download a source package
via the link above and unpack the contents.

Secondly, you will need a C compiler toolchain which is C99 compliant.
I have personally tested GCC, clang and TCC.  That's it really, there
are no other dependencies.

For Windows, I use a cross-compiler (*MinGW-w64*) under Linux to build
the Windows binary.  I have not tested using a Microsoft compiler.

Compiling is as simple as typing `make` inside the source directory.


Editor Support
--------------

There is a syntax highlighting file for the editor *Vim* in the `misc/`
directory of this repository.
