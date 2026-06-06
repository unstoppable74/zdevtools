It is assumed that the user has a good working knowledge of the Z-machine; this
document is not meant as a tutorial, nor is this assembler meant to be useful
for anything beyond the testing of interpreters.

All Z-machine opcodes for all Z-machine versions are implemented. However, there
are some major features of the Z-machine that are not currently implemented.
These include (but surely are not limited to) objects, the dictionary, and
abbreviations.

Unicode tables are supported, to some degree. If non-ZSCII characters are
encountered in strings, they are added to the Unicode table. Note that a new,
custom Unicode table will always be created: even if you only use characters
present in the Z-Machine's standard Unicode table, a new table with only the
characters you use will be created. This may be changed in the future, but will
require significant work.

Alternatively, you can provide your own Unicode table. See the `unicode_table`
directive below.

Because this is a single-pass assembler, the writing of the Unicode table is
delayed till the end of the file, and written at the end of the output; this is
due to the fact that the size of the Unicode table won't be known till assembly
is done. However, the Unicode table must be at a 16-bit address, so if your
program extends beyond that, assembly will fail. You can get around this with
the --preallocate-unicode-table flag. This will preallocate space near the
beginning of the file for up to 97 Unicode characters (the max). If you have
fewer than 97, this will waste some space, but at least it will assemble.

There is no direct access to the Unicode table, but if you use
UTF-8 characters in a string, they will automatically be added to the Unicode
table.

Diagnostics are generally useful, but might sometimes be cryptic. If all else
fails, look at the source code. This assembler is quite rough around the edges.

Source files must be encoded in UTF-8. If you're only using ASCII, that will be
fine.

In this file, the term "C-style constant" refers to a number as written in C: a
leading 0x means the number is hexadecimal, otherwise, decimal. Octal and binary
constants are not supported.

Opcode names are identical to those given in §15 of the Z-machine standard.
Unlike Inform, opcodes are not prefixed with @.

Comments are introduced with the # character and extend to the end of the line.
Comments must begin in the first column of the line.

The `align` directive, when given no arguments, forces routine alignment,
ensuring that the next instruction is on a 2-, 4-, or 8-byte boundardy,
depending on the Z-Machine version (1-3, 4-7, and 8, respectively). When given a
C-style constant as an argument, align to that value instead:

    align
    align 0x10

A label is introduced with the `label` directive:

    label LabelName

An aligned label (which is the same as calling `align` then `label`) has the
same syntax:

    alabel ALabelName

A routine is introduced with the `routine` directive, and includes the number of
locals the routune has (this cannot be omitted even if there are no locals):

    routine RoutineName 5

If a version 1-4 story file is being created, initial values can be given to
each local variable by listing their values after the number of locals. The
following gives the value 1 to L00, 2 to L01, 3 to L02 and, because values were
omitted, L03 and L04 are set to zero:

routine RoutineName 5 1 2 3

An arbitrary byte sequence is introduced with the `byte` directive, each byte
specified as a C-style constant, separated by space:

    byte 0xfa 0xff 0xfa

The `seek` directive inserts the specified number of zeros into the output
file; the argument is a C-style constant:

    seek 0x100

The `seeknop` directive is identical to `seek`, except instead of zeros, `nop`
instructions are inserted.

The `seekabs` directive inserts enough zeros to ensure the next instruction is
located at the specified offset. This can be used to place something (e.g. a
string) at a specific location in memory. This will fail if an offset is given
that would come before the current location in the file.

    seekabs 0x1000
    string "This string is now at location 0x1000 in memory"

The `seeknopabs` directive is identical to `seekabs`, except instead of zeros,
`nop` instructions are inserted.

The `status` directive, available only in V3 stories, indicates whether this is
a "score game" or a "time game". The syntax is:

    status score
    status time

The `status` directive may be specified at any point in the source file any
number of times. The last takes precedence. The default game type is a score
game. Although objects are not supported by this assembler, object 1 is created
so that interpreters can properly display the status bar. This object has the
name "Default object" and no properties.

The 'unicode_table` directive inserts the Unicode table at the current location.
It takes either a collection of space-separated numbers or a string. A Unicode
table will be inserted at the current location, and that location will be
written to the header extension table. The syntax is one of:

    unicode_table "æč"
    unicode_table 0xe6 0x10d

This table will entirely replace the default table provided by the assembler.

The opcodes `print` and `print_ret` take a quoted string as their argument; to
include literal quotation characters, they must be escaped with a \ character as
in C. To include a literal backslash, it must be escaped (\\). To include a
newline, use the ^ character, as in Inform. A literal ^ cannot currently be
encoded. Example:

    print "\"Try\" our hot dog's^Try \"our\" hot dog's^"

The directive `string` is treated in the same manner. This directive will
directly insert an encoded string, suitable for use with `print_addr` or
`print_paddr`. Note that `print_paddr` requires strings to be aligned due to the
fact that it expectes a packed address, so `alabel` should be used with
`print_paddr` instead of just `label`:

    label s1
    string "print_addr^"
    alabel s2
    string "print_paddr^"
    start
    print_addr s1
    print_paddr s2
    quit

`print_char` can take literal characters (surrounted by single quotes) as well
as any other expected value. Characters must be valid printable ZSCII, which is
to say characters in the range 32-126.

    print_char 'A'
    new_line
    store G00 65
    print_char G00
    new_line

The `aread` and `sread` opcodes are just called `read` in this assembler.

If you are in a routine, local variables can be accessed as `L00`, `L01`, ...
`L15`. The numbers are decimal.

Global variables are available in `G00`, `G01`, ... `Gef`. The numbers are hex.

The stack pointer is called `sp` or `SP`.

For opcodes that require a store, use `->` to indicate that:

    call_1s Routine -> sp

Literal numbers are stored appropriately, meaning as a small constant for values
that fit into 8 bits, a large constant otherwise. Only values that will fit
into a 16-bit integer (signed or unsigned) are allowed, meaning -32768 to 65535.
Numbers are parsed as C-style constants.

The `start` directive must be used once, before any opcodes are used. Until
`start` is seen, the assembler writes to dynamic memory. Once `start` is
encountered, static memory begins, and the starting program counter is set to
that address. The reason for this is chiefly to allow arrays to be placed in
dynamic memory:

    # Combine label and seek to produce arrays.
    label Array
    seek 100
    # Execution will begin here.
    start
    storew Array 0 0x1234

In V6, the starting point of the story must be a routine, not an instruction.
To accomplish this, `start` has a special syntax for V6, which looks identical
to the syntax for `routine`:

    start main 0

This causes a routine called `main` with zero locals to be created, and uses
this as the entry point. These arguments are allowed in non-V6 games, but are
ignored.

Labels are used to give names to addresses so they can be referred to in
instructions. However, there are some opcodes (such as `print_paddr`) which take
a packed address. As such, for opcodes which require packed addresses, the
assembler will automatically pack labels.

    # No packing necessary
    print_addr label_name

    # This is packed automatically
    print_paddr label_name

So these opcodes will be passed different values. Note that the name of a
routine is a label as well, and will be packed or not depending on the opcode.
This also means you can pass a routine label to `print_paddr`, but you probably
shouldn't.

In addition to packed targets, branching has its own method of encoding
addresses. You cannot simply pass a label to a branch:

    # Invalid
    je 0 0 label_name

This is because of the complexity of branch. There are several questions to ask
about a branch:

* Is this a short (6-bit) or long (14-bit branch)?
* Is this inverted, i.e. does false cause the branch to be taken?
* Instead of branching to a label, is this returning true/false?

There are 8 possible combinations, which is why a simple label is not
sufficient. For that reason, sigils are used to determine how the branch is to
be made. These are:

* ``?label_name``: 14-bit branch to label if true
* ``?~label_name``: 14-bit branch to label if false
* ``%label_name``: 6-bit branch to label if true
* ``%~label_name``: 6-bit branch to label if false
* ``?0``: return false if true
* ``?~0``: return false if false
* ``?1``: return true if true
* ``?~1``: return true if false

So, for example:

    je 0 0 ?label_name
    je 0 0 ?~0

For both the 6- and 14-bit branches, the assembler will tell you if you're
trying to jump too far, but it won't rewrite a 6-bit branch into a 14-bit one.

If you are using an opcode which takes both a store and a branch (`get_sibling`,
`get_child`, or `scan_table`), you must put the store before the branch, e.g.:

    get_sibling G00 -> sp ?1

Labels cannot be used with arbitrary opcodes since they don't make sense in most
contexts. However, if you do want to pass an address to an opcode that wouldn't
normally take an address, you can prefix the label to indicate your intent:

    # This passes an unpacked label, so you could print the address of an instruction
    label label_name
    print_num &label_name
    
    # This passes a packed label
    print_num !label_name

Note that routine and string offsets (for V6 and V7 games) are not supported, so
the packing of strings and routines at the top of memory (beyond 256K) will
fail. This should not be an issue unless you deliberately seek this far before
creating a string or routine.

Here is a full working sample:

    start

    call_1n main
    quit

    routine main 0
    je 0 0 ?Equal
    print "This will not be seen.^"
    label Equal

    jump Past
    print "This will not be seen either.^"
    label Past

    print "The main routine is: "
    print_num &main
    new_line
    print "Packed, the main routine is: "
    print_num !main
    new_line
    print "The Equal label is: "
    print_num &Equal
    new_line

    quit
