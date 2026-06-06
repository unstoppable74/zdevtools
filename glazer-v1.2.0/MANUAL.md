
GLAZER MANUAL
=============

by Andrew Apted, 2025.


Preface
-------

This manual describes the syntax of assembly code which is accepted
by Glazer.  The Glulx VM and the GLK I/O system are well documented
by their own specifications, and familiarity with them will be assumed
here.

Some of the keywords and pseudo-instructions used by Glazer are
inspired by the *NASM* assembler, which I have personally used a lot
in the past.  Other things, like the usage of the `->` symbol, are
purely my own kink.  Compatibility with Inform assembly has not been
a goal here.  For example the `sp` keyword is not accepted.


General Usage
-------------

Glazer is a command-line program, so you should run it from a text
terminal or console.  If any errors are found in the source file, they
will be displayed on the terminal (usually showing the line number
where the problem occurred).

Showing the brief built-in help:
```
    > glazer --help
```

Compiling the samples:
```
    > glazer hello.asm
    > glazer life.asm -o othername.ulx
```

The `-o` option allows specifying the exact output filename.
When absent, the output will have the same name as the input file
but with the extension changed to ".ulx".

The `-s` option can be used to dump the symbol table (all the labels
to be precise) after a successful compile.  The symbols are shown in
order of address, and are separated into the three sections (text,
data and bss).


Assembly Files
--------------

There is no official file extension for Glazer source files.
The samples here use ".asm" which is a fairly generic extension
which many assemblers use.  Glazer itself does not care, so you
can use whatever you like.

The overall format is line-based, and we assume the UTF-8 encoding.
Unicode characters may be used in both strings and identifiers.
Lines may be indented with either spaces or tabs, however this is
purely for readability and is not required by the syntax.

See the [GRAMMAR.txt](GRAMMAR.txt) document for the formal grammar
of Glazer code.  It uses EBNF notation.


Comments
--------

Comments begin with a semicolon (`;`) and extend to the end of the
line.  They may occur one a line by themselves, or after a normal
line of code.  They **cannot** occur in the middle of a string in
double quotes, or a character literal in back-quotes.


Identifiers
-----------

Identifiers consists of the following characters:

- letters (ASCII, Latin-1 or Unicode)
- digits  (`0` .. `9`)
- the underscore (`_`)
- the dot (`.`)

The only restriction is that identifiers cannot begin with a digit.

Identifiers beginning with a dot have a special meaning for labels,
but are not allowed for constants.


Numbers
-------

There are two types of numeric literals: integers and floating point.
They are distinguished by including a dot (`.`) or the letter `e` (or `E`)
to get a floating point literal, otherwise the number is an integer.

Numbers may begin with a sign, either a plus (`+`) or a minus (`-`).
This is considered part of the numeric literal, hence you need to be
careful in expressions, because something like "(x+3)" will **not**
be accepted, it will need to be written "(x + 3)" with spaces around
the plus operator.

Integer literals may include spacers for better readability, which are
the underscore (`_`) and apostrophe (`'`) characters.  These are not
allowed in floating point constants

Integer literals may be written in hexadecimal, using the `0x` prefix,
or in binary, using the `0b` prefix.

Floating point literals must always start with a digit, things like
".123" or "e7" are not accepted.  Floating point literals may include
an exponent, which is the letter `e` (or `E`) followed by an optional
sign, then a series of decimal digits.

Example:
```
    123456789    ; an integer
    123_456_789  ; the same integer with spacers
    -123456      ; an integer with a sign
    0xDEADBEEF   ; a hexadecimal integer
    0b11101010   ; a binary integer

    +1234.56     ; a floating point number
    +1.23456e+3  ; the same number using an exponent
```


String Literals
---------------

String literals begin with a double quote (`"`), and are terminated
with the same symbol.  To include a double quote in the string, an
escape sequence must be used (see below).

String literals cannot span multiple lines, they must be terminated
on the same line as they began.  To include long passages of text,
it is **necessary** to use complex strings (described in a section
below), since Glazer does have a limit on the length of an input line,
albeit a fairly high limit.

Strings may only be used with the data and string pseudo-instructions,
namely `db`, `dw`, `dd`, `latstr`, `unistr` and `huffstr`.  They may
be used inside a complex string.  They **cannot** be used as operands
to an instruction, or as the value of a user-defined constant.

Example:
```
    section     .data
    raw_str:    db      "this is a raw string (not a Glulx object)" 0
    obj_str:    latstr  "this is a Glulx object, the NUL is implied"
    comp_str:   latstr  { "this is" " a " "complex string" }
```


Character Literals
------------------

Character literals are a single (logical) character enclosed by
back-quotes.  To include a back-quote itself requires the usage of
an escape sequence (see below).

Character literals are mostly equivalent to an integer with the
same value as the character's code in Unicode.  However, character
literals make code easier to read, and they behave differently to
integers when inside a complex string.

Example:
```
    section     .data
    raw_string:  db  `H` `e` `l` `l` `o` `\n` 0
```


Escape Sequences
----------------

Escapes serve two purposes: firstly to introduce a character which
would otherwise be impossible, such as a double quote in a string
literal, and secondly to produce arbitrary Unicode characters.

Escapes are introduced with a backslash (`\`) and are followed
either by a single symbol/punctuation character, which is used to
escape that particular character, or by an ASCII letter and possibly
extra stuff, which has a special meaning.

The letter `n` character after the backslash produces a new-line
character: ASCII code 10 or U+000A in Unicode.

The letter `u` character after the backslash produces an arbitrary
Unicode character.  It must be followed by an opening curly brace
(`{`), then one or more hexadecimal digits, and then a closing curly
brace (`}`).  Every character is allowed except NUL (U+0000) which is
reserved for usage as the string terminator.

Example:
```
    unistr  "this string ends with new-line\n"
    unistr  "this \"string\" contains double quotes"
    unistr  "this string has a smiley: \u{263B}"
```


Built-in Constants
------------------

The following constants are built-in since they are so useful:

- `FALSE` and `TRUE` are for boolean states.  They are equivalent to
  the integers `0` and `1` respectively.

- `NULL` is often used to mean "no value" when passing a pointer to or
  from a function call.  It is equivalent to the integer `0`.

- `STACKREF` is for GLK calls where the parameter is on the stack.
  It is equivalent to the integer `-1` (or 0xFFFFFFFF when unsigned).

- `NAN` is a special floating point constant which means "Not a Number".
  It can occur during math operations where the result makes no sense,
  for example when dividing zero by zero.  As a constant, it is rarely
  useful, but it is provided for completeness.

- `INFINITY` is another special floating point constant representing
  positive infinity.  To get negative infinity, use the expression
  `(- INFINITY)`.  Infinities can occur during math operations, for
  example when dividing a non-zero number by zero.  As a constant,
  it is rarely useful, but it is provided for completeness.


File Inclusion
--------------

The `include` directive allows another assembly file to be read and
its contents become part of the current file.  It is followed by a
string which is the filename of the file to include.  Note that to
include backslashes (`\`) in the filename, they need to be escaped
using a pair of backslashes (`\\`).

File inclusion can be useful for splitting up a large project into
smaller, more manageable pieces, and also for storing a useful set of
definitions (e.g. for the GLK I/O system) in a common file shared by
many projects.

There is a search path for included files.  The first place looked
for a non-absolute filename is in the same directory as the primary
input file.  Additional directories can be appended to this search
path via the `-p --path` command-line option, and these will be
checked in the order given.

By convention, the `include` directive is not indented.

Example:
```
    ; read common defs
    include  "my_defs.asm"
```


Sections
--------

The `section` directive changes the current section.  It is followed
by one of the following section names:

- `.text` which represents the read-only (ROM) area
- `.data` which represents the read-write (RAM) area
- `.bss` which represents the RAM extension area

The current section is where each instruction (or chunk of data) will
be stored in the generated output file.  Nothing can be stored in the
bss section, the only thing supported there is reserving some space.

By convention, the `section` directive is not indented.

Example:
```
    section     .text
    my_func:    function
                ; some code...

    section     .data
    my_msg:     latstr  "Hello there\n"

    section     .bss
    my_var:     resd  1
```


Defining Constants
------------------

A constant is defined by specifying its name followed by an equals
sign (`=`) and then the value.  The value may be a number or character
literal, or an expression in `()` parentheses, but it **cannot** be a
string.

Constants may be used before they are defined, but for readability
it is recommended they appear as early as possible in an assembly
file.

By convention, a constant definition is not indented.

There is one special constant you can define: `stack_size`, which sets
the size of the stack of the Glulx VM (in bytes).  The default is 65536,
which is usually plenty, but you can be change it if needed.

Example:
```
    ; size of screen
    SCREEN_WIDTH  = 80
    SCREEN_HEIGHT = 25
    SCREEN_SIZE   = (SCREEN_WIDTH * SCREEN_HEIGHT)
```


Defining Labels
---------------

A label is defined by specifying its name and following it with the
colon symbol (`:`).  By convention, labels start at the beginning of
a line, and while two or more labels may appear on the same line, this
should generally be avoided for the sake of readability.  A label may
occur on a line by itself, or may be followed by an instruction or
pseudo-instruction.  Labels **cannot** occur in the middle of an
instruction, or following it.

The value of a label is the final address of that particular point
of the assembly code, and implicitly refers to the section containing
the label.  Global labels must be unique, they cannot be redefined.

Labels which begin with a dot (`.`) are *local labels*.  Such labels
are not global, they belong to the most recently declared function,
and can only be referred to by code in that function.  Since these
labels are local to a function, multiple functions can use the same
names without any chance of a name clash.

There is one special label which you **must** define somewhere in the
assembly code: `entry_point`.  It tells the Glulx VM where to begin
executing your code, and it must refer to a Glulx function object.

Example:
```
    my_awesome_func:
                function
                ; some code...
    .loop:
                ; more core...
                jump  .loop
```


Functions
---------

The `function` pseudo-instruction begins a new function, and is used
for functions which pass their parameters via local variables.  They
generally have a fixed number of parameters.

The `func_va` pseudo-instruction is an alternate type of function
which pass their parameters via the stack.  The "va" is short for
"variable arguments", since these functions may be called with any
number of arguments, the exact number a function gets is also stored
on the stack.

These instructions create a Glulx function object, and Glazer takes
care of storing the object byte at the beginning, as well as the data
describing the local variables.  Nifty, right?

Caveat: since a function begins with a little header, you **cannot**
directly jump to it in the body of a function (e.g. for looping).
You will need to add a new label to jump to.


Local Variables
---------------

Each function may have one or more *local variables*.  These are
defined by the `local` pseudo-instruction, which is followed by one
or more names.  This can be used anywhere after defining a function,
though for parameters I suggest using it on the very next line.

Each local variable can store a single 32-bit value
(there is no support for 8-bit and 16-bit locals, which have been
deprecated by the Glulx specification).

The names of local variables can be used in instructions, both as
load operands and as store operands.  The names are always local to
the current function, and never interfere or clash with the local
variables of another function.

Example:
```
    show_the_answer:
                function
                local      the_answer
                copy       42 -> the_answer
                streamnum  the_answer
                return
```


Instructions
------------

Instructions make up the bulk of code in an assembly file.  They
consist of the opcode name, such as `add`, then zero or more load
operands, then zero or more store operands, and finally there may
be a jump target.  When there are store operands, they **must** be
preceded by the arrow (`->`) symbol, and a jump target must also be
preceded an arrow.

By convention, instructions are indented (using either spaces or tabs)
except when preceded by a label.  The operands are usually offset from
the instruction at a fixed column, which is helpful for readability.

The store operands (and the `->` symbol) may be omitted, in which case
all the store operands become the `drop` operand, i.e. no actual store
will be performed.  Note that the `catch` instruction may not omit the
store, it is the only instruction which has both a store and a jump
target, and both must be present.

A jump target must either be a label name, or the special keywords
`rfalse` or `rtrue`.  These keywords cause the function to return
instead of jumping, where the returned value is 0 for `rfalse` and 1
for `rtrue`.  Note that the `jumpabs` instruction is different, it
takes a normal operand (the computed address), and **does not** need
or allow the arrow symbol, nor does it accept the `false` or `rtrue`
keywords.

The `return` instruction usually requires a load operand for the
value to return.  However many functions do not return a meaningful
result, and for these the value can (and should) be omitted, in which
case it defaults to zero.

Example:
```
    add_func:   function
                local      sum
                add        1 2 -> sum   ; add two numbers, store in 'sum'
                streamnum  sum          ; show the value of 'sum'
                return                  ; return from the function
```


Operands
--------

Operands give the information which instructions will load and store to
perform their task.  The following are the available kinds of operands:

- literal values, like `123`
- expressions, like `(1 << 16)`
- constant names, like `glk_window_open`
- local variable names, like `win_id`
- label names, like `message_str`
- locations in memory, like `[my_var]`
- loading from the stack: `pop`
- storing onto the stack: `push`
- storing nowhere: `drop`

Note that strings **cannot** be used as an operand.

The `pop` keyword may only be used for load operands, and the `push`
and `drop` keywords may only be used for store operands (after the `->`
arrow symbol).  The `drop` keyword is rarely needed, since most
instructions allow the store operand(s) to be omitted.

A location in memory requires a value or expression inside square
brackets (`[]`).  The value could be an absolute address, like `0x0024`,
but it will usually be a label, or an expression containing one or more
labels.  The most common kind of expression is using an offset, such as
`(some_label + 12)`, but any of the math operations can be used.

Operands to instructions are limited to 32-bit values.  When using
double precision floating point, this makes using literals difficult.
The special operand `hilo` is provided to help here, it **must** be
followed by a floating point value, and Glazer will take care to
split the 64-bit value into two 32-bit values (raw integers).

Example:
```
    some_func:  function

                ; initialize my structure
                copy     0 -> [my_struct + 0]
                copy  1234 -> [my_struct + 4]
                copy   -37 -> [my_struct + 8]

                ; call a function and pass the structure to it
                push   my_struct
                call   another_func 1

                ; store PI in a double float variable
                local  num_hi num_lo
                dcopy  hilo 3.1415926535897932384 -> num_lo num_hi

                ; add one to that variable
                dadd   num_hi num_lo  hilo 1.0 -> num_lo num_hi

                ; call a function to print it
                dpush  num_hi num_lo
                call   print_float 2

                return
```


Fake Instructions
-----------------

Glazer supports a few "fake" instructions.  These are like macros
which expand to one or more real instructions when used.  Some of
them are designed to ease the excruciating pain of working with
double precision floating point (at least a little bit...)

The following are the available fake instructions:

```
push L1
```

Pushes a value onto the stack.  Useful to push parameters when
calling a function.  Equivalent to using the `copy` instruction
with `push` as the store operand.

```
pull -> S1
```

Pops a value from the stack.  Equivalent to using the `copy`
instruction with `pop` as the load operand.

```
dpush Xhi Xlo
```

Pushes a pair of values onto the stack.  Equivalent to using
two `copy` instructions.  Can be used with the `hilo` syntax
to push a double precision FP literal onto the stack.

```
dpull -> Ylo Yhi
```

Pops a pair of values from the stack.  Equivalent to using
two `copy` instructions.

```
dcopy Xhi Xlo -> Ylo Yhi
```

Copies a pair of values.  Equivalent to `copy Xhi -> Yhi` and
`copy Xlo -> Ylo`, though this is done in the opposite order
when the store operands are `push`.  Can be used with the `hilo`
syntax to copy a double precision FP literal into a pair of local
variables (etc).

It is highly recommended that if one operand is a `pop` or `push`,
they *both* are.  This may be enforced in the future.

```
dload L1 -> Ylo Yhi
```

Reads a pair of values from memory address L1.  This is equivalent
to two `aload` instructions.

```
dstore L1 Xhi Xlo
```

Writes a pair of values to memory address L1.  This is equivalent
to two `astore` instructions.  Can be used with the `hilo` syntax to
write a double precision FP literal into memory.


Data
----

Data may be stored in the text or data sections of the output file.
The following four pseudo-instructions produce elements of a fixed
size:

- `db` : store a series of  8-bit bytes
- `dw` : store a series of 16-bit words
- `dd` : store a series of 32-bit dwords
- `dq` : store a series of 64-bit quads

These pseudo-instructions are followed by one or more values,
separated by spaces.  Each value can be an integer or float literal,
a character literal, a string literal, the name of a label or constant,
or an expression in `()`.

The values are checked to make sure they can be represented in the
destination size.  This check is a bit lenient, they allow the value
to be represented as either a signed or unsigned value.  For example,
the `db` instruction requires 8-bit values, so it allows values in the
range -128 to 255, but will reject values like -131 and 264.

When strings are used with these four pseudo-instructions, it is
equivalent to breaking the string up into the correct number of
character literals.  In an 8-bit context, Unicode points > U+00FF are
**not** accepted, similarly for points > U+FFFF in a 16-bit context
(there is no support for surrogate pairs).  Note that strings are not
inherently NUL-terminated, i.e. no zero value is automatically added
after the final character with these instructions.

The following three pseudo-instructions are more commonly used for
strings, because they create a true Glulx string object:

- `latstr`  : for strings using U+0001 to U+00FF
- `unistr`  : for strings using the full range of Unicode
- `huffstr` : for Huffman encoded (compressed) strings

These instructions must be followed by a single thing: either a string
literal, or a complex string in `{}`.  See the section below for more
information about complex strings.  Glazer will take care of using the
correct Glulx object type at the beginning, and terminating the string
with NUL (zero) at the end.

When present, all the `huffstr` strings (simple or complex) will be
analysed and a Huffman decoding table will be produced in the output
file.


Reserving Space
---------------

The bss section can only be used to reserve space, never to store
instructions or data, since this area of the memory map simply does
not exist until a Glulx VM loads the file and creates it.

Space may also be reserved in the other sections.  In the data and
text sections, it is equivalent to filling the area with zeros.

Reserving space is done using the following pseudo-instructions.
Each one has an intrinsic size, e.g. bytes for `resb`, and must be
followed by a single integer to reserve that many units.

- `resb` : reserve a series of  8-bit bytes
- `resw` : reserve a series of 16-bit words
- `resd` : reserve a series of 32-bit dwords
- `resq` : reserve a series of 64-bit quads

Example:
```
    section     .bss
    win_id:     resd  1    ; a variable to store the window ID
```


Alignment
---------

Labels in any section may be aligned to a multiple of a specific value
using the `align` pseudo-instruction.  It is equivalent to reserving a
the number of bytes (possibly zero), enough to guarantee the wanted
alignment.  The value is typically a power-of-two, but other values
are allowed.

Example:
```
    section     .bss
    var_1:      resb   1     ; this variable is located at 0x0480
                align  4
    var_2:      resb   4     ; this variable is located at 0x0484
                align  256
    var_3:      resb   200   ; this variable is located at 0x0500
```


Complex Strings
---------------

Complex strings are introduced by an opening curly brace (`{`) and
extend to a matching closing curly brace (`}`).  The contents may
span multiple lines.

They may contain the following elements:

- a string literal
- a character literal
- an integer or float literal
- a constant name
- an expression in `()`
- an indirect reference in `[]`

String literals are the most common thing, and complex strings are
useful for splitting a long passage of text into a group of shorter
strings.

A character literal in a complex string is equivalent to a string
literal containing a single character.

An integer or float literal, or a constant or expression which
evaluates to an integer or float, will be converted to a string.
For example, the number 123 simply becomes the string `"123"`.
There is no support for controlling how this conversion occurs.

Indirect references occur inside square brackets `[]`, and are used
to display another Glulx object, which may be a string object or a
function object to call.  The first element of an indirect reference
is the address of the object (usually a label).  Following the address
may be one or more arguments for a function call.

There is also a "double indirect" feature: when the first element
inside `[]` is a memory pointer using square brackets, then the
address of the object is not directly specified, instead it refers
to a field in memory, and *that* contains the address of a string or
function.

While complex strings may be used with the `latstr` and `unistr`
pseudo-instructions, a complex string containing indirect references
**can only** be used with the `huffstr` pseudo-instruction, since only
Huffman encoded strings have the mechanism needed to display them.

Example:
```
    string_1:   latstr  { "example " "number #" 1 `!` `!` }
    string_2:   huffstr { "show string_1: " [string_1] }
    string_3:   huffstr { "show string in a var: " [[my_var]] }
    string_4:   huffstr { "call a function: " [my_func 123 456] }
```


Expressions
-----------

Expressions are introduced by a left parenthesis (`(`) and extend to
a matching right parenthesis (`)`).  They may contain just a single
value, but usually contain a mathematical expression to compute a new
value consisting of operators and terms.  They may span multiple lines.

A memory operand inside `[]` can also be an expression.  See the
section on operands for more details.

The terms of an expression can be literal values, including integers
and floats, as well as the names of user-defined constants and labels.
Sub-expressions in `()` may also be used.

Terms may be preceded by one of the following unary operators:

- `+` unary plus, no change to the following value
- `-` unary minus, negate the following value
- `~` bitwise negation of the following value

The following binary operators are available:

- `+`   addition of two terms
- `-`   subtraction of two terms
- `*`   multiplication of two terms
- `/`   division of two terms
- `%`   remainder (modulo) of a division
- `<<`  left shift of an integer
- `>>`  unsigned right shift of an integer
- `>>>` signed right shift of an integer
- `&`   bitwise AND of two integers
- `|`   bitwise OR  of two integers
- `^`   bitwise XOR of two integers

Each binary operator has a precedence, and expressions are parsed in a
way which takes precedence into account.  For example `(1 + 2 * 3)`
will produce the expected value 7.  Note that the precedence values
are *different* than with C and C++.  When using the bitwise operators
and/or the shift operators, it is recommended to use parentheses to
make it clear what is meant.

While all of the above operators may be used with integers, only the
following may be used with floating point values: `+ - * / %`

Each operator should produce the same results as the equivalent Glulx
instruction.  For example, using `%` with floating point computes the
result using *truncated* division, as per the `fmod()` function in C.
Note that integer division by zero is considered an error (the program
will fail to compile).  For floating point, operations like that are
allowed, they generate the special NAN and/or INFINITY values.

Example:
```
    value_1 = (1 + 2 * 3)        ; should be 7
    value_2 = (value_1 & 9)      ; should be 1
    value_3 = (1 << 16)          ; should be 65536
    value_4 = ((- 32) >>> 3)     ; should be -4
```

Z-Code Differences
------------------

The Z-machine is quite different than the Glulx VM.  One major
difference is that the Z-machine operates on 16-bit words.  Local
variables are always 16-bit, and none of the opcodes support 32-bit
operations.

### Sections

The Z-machine has three primary sections, called "dynamic", "static"
and "high" memory in the specifications.  Dynamic memory occurs at
the bottom of the address space, and is read/write.  Static memory
occurs above dynamic memory, but is read-only.  High memory contains
functions and strings, and cannot be directly accessed by the
load/store opcodes.

The following section names are available:

- `.data`   represents the dynamic (read-write) area
- `.static` represents the static (read-only) area
- `.text`   represents the high area, for functions and strings

The total amount of dynamic and static memory is limited 65534 bytes.
The limit for high memory depends on the target Z-machine version:

### Entry Point

As with Glulx, the special `entry_point` label **must** be defined
somewhere in the assembly code.  For all Z-machine targets except V6,
the real entry point will be a small bit of code generated at the
start of high memory.  That code merely calls your `entry_point`
function, and quits the interpreter should it return.

### Header Fields

TODO

`release` directive TODO

### Global Variables

Global variables are defined by the `global` directive, which may
be used anywhere in the source assembly.  Global variables may be
used before they are defined.  For readability it is recommended
to define them early in the assembly file, e.g. after constants.

Global variables can be given a default value by following the name
with an equal sign (`=`) and then the value.  The value may be a number
or character literal, or an expression in `()` parentheses.  When no
value is given, it defaults to zero.

Example:
```
    global found_the_key          ; defaults to 0
    global light_remaining = 400  ; has an explicit value
```

### Instructions

The format of instructions is mostly the same as with Glulx.
Of course, the Z-machine has its own set of opcodes.  Note that
not all opcodes are available for all Z-machine targets.

One difference is that two instructions, `print` and `print_ret`,
require an operand which is a literal string.  This string becomes
part of the generated code.

Another difference is that instructions which perform a store
**cannot** omit the store (the `->` must be present), and the `drop`
operand is not supported.  Most call instructions have a variant
without the store, and with V3 you can store to the stack and then
`pop` it.  Everything else requires using a local or global var.

For compatibility, "ret" is a synonym for the "return" instruction.
As with Glulx, this instruction may also omit its operand, in which
cause it defaults to zero.

For jump instructions, a new keyword is available: `fail`.  This can
be used before the `->` symbol, and means the jump is taken when the
instruction fails (rather than when it succeeds).

### Operands

Memory access in `[]` is not available.

Floating point values and the `hilo` keyword are not usable.

### Data

TODO
