# Bude
Bude is a stack-based language inspired in part by [Porth](https://gitlab.com/tsoding/porth).

NOTE: The language is currently very unfinished.

Assembly code can be generated and assembled by [FASM](https://flatassembler.net/).

## Building

The project can be built using the makefile provided. This may need to be edited to configure
for different environments (e.g. to change to compiler other than GCC).

```shell
$ make
```

## Running the compiler

After building the project, the interpreter can be run using the following command:

```shellsession
$ ./bin/bude.exe ./examples/hello.bude
Hello, World!
```

This will run the program `hello_world.bude` from the `examples` subdirectory. A full list of
command line options can be found by running:

```shell
$ ./bin/bude.exe -h
```

To create a Windows executable file, run:

```shellsession
$ ./bin/bude.exe ./examples/hello_world.bude -a -f hello_world.asm
$ fasm hello_world.asm
$ ./hello_world.exe
Hello, World!
```

This assumes FASM has been installed and is in the PATH variable.

## Language Features

Bude has a stack for storing 64-bit words. There are instructions to manipulate the stack.

Notation: below, different annotations are used to describe how data on the stack should be
interpreted for the differet instructions. Note that regardless of the size of the underlying
C type, these values always take up one full stack slot (64 bits). A C-like language is used
for the result expressions of some operations.

* _w_ &ndash; arbitrary stack word
* _i_ &ndash; signed integer value
* _p_ &ndash; pointer value
* _c_ &ndash; ~~UTF-8~~ ASCII codepoint (will be UTF-8 in the future)
* `<literal>` &ndash; the literal value denoted

### Push instructions

`"Lorem ipsum"` &rarr; _p_ _i_ : Push the specified string to the stack
along with its length.

`42` &rarr; _i_ : Push the specified integer to the stack.

### Pop instructions

_w_ `pop` &rarr; &varnothing; : Pop the top element from the stack and discard it.

_i_ `print` &rarr; &varnothing; : Pop the top element from the stack and print it as an integer.

_c_ `print-char` &rarr; &varnothing; : Pop the top element from the stack and print it as a
unicode character.

### Arithmetic operations

_i1_ _i2_ `+` &rarr; (_i1_ + _i2_) : Pop the top two elements and push their sum.

_i1_ _i2_ `-` &rarr; (_i1_ - _i2_) : Pop the top two elements and push their difference.

_i1_ _i2_ `*` &rarr; (_i1_ \* _i2_) : Pop the top two elements and push their product.

_i1_ _i2_ `/%` &rarr; (_i1_ / _i2_) (_i1_ \% _i2_) : Pop the top two elements and push their
quotient and remainder.

### Logical operations

_w_ `not` &rarr; !_w_ : Replace the top element with its logical inverse (i.e. non-zero &rarr; 0,
0 &rarr; 1).

_w1_ _w2_ `or` &rarr; (_w1_ or _w2_) : Drop _w1_ if it is equal to zero, else drop _w2_.

_w1_ _w2_ `and` &rarr; (_w1_ and _w2_) : Drop _w1_ if it is non-zero, else drop _w2_.

### Memory operations

_p_ `deref` &rarr; (byte \*_p_) : Pop the pointer _p_ and push the first byte it points to.

### Stack manipulation

_w1_ _w2_ `swap` &rarr; _w2_ _w1_ : Swap the top two elements on the stack.

_w1_ `dupe` &rarr; _w1_ _w1_ : Duplicate the top element on the stack.

### Conrol flow constructs

`if` _condition_ `then` _then-body_ [`elif` _elif-condition_ `then` _elif-then-body_ &hellip;]
[`else` _else-body_] `end`

`while` _condition_ `do` _body_ `end`

`for` _count_ `do` _body_ `end`
