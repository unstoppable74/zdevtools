
CHANGELOG
=========

v1.2.0 : 2025-Dec-11

- new `align` pseudo-instruction
- support all the math operations in label expressions
- allow labels to be used with `dw`
- allow custom names (labels, locals, etc) to be same as an instruction

v1.1.0 : 2025-Sep-08

- raised limit on number of locals in a function, from 255 to 16000
- massive speed up of assembling large files via better Node code
- fixed not showing symbols for `-s` option when using `-q` option
- removed a few chars (like U+200C) from being considered whitespace

v1.0.0 : 2025-Aug-15

- support a file inclusion search path, new `-p --path` option 
- allow `return` instruction to omit its value, default to zero
- added `-q --quiet` command-line option
- fixed wrong info in the manual about `latstr` and `unistr`

v0.9.0 : 2025-Jul-29

- wrote a very fine manual
- support for floating point literals and expressions
- new `hilo` syntax for float literals in double precision instructions
- new fake instructions: `dcopy`, `dload`, `dstore`, `dpush`, `dpull`
- added float constants `NAN` and `INFINITY`
- added integer constants `FALSE`, `TRUE`, `NULL`, and `STACKREF`
- check new constant/label names don't match instructions or keywords
- require section names to have the dot prefix (`.`)

v0.7.0 : 2025-Jul-25

- implemented complex string syntax and huffman encoded strings.
- check the operands of the `getiosys` instruction
- fixed crash bug when using a memory operand with an offset
- new option `-s --syms` to dump the final symbol table

v0.6.0 : 2025-Jun-12

- first public release
