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
$ ./bin/bude.exe ./examples/hello_world.bude
Hello, World!
```

This will run the program `hello_world.bude` from the `examples` subdirectory. A full list of
command line options can be found by running:

```shell
$ ./bin/bude.exe -h
```

To create a Windows executable file, run:

```shellsession
$ ./bin/bude.exe ./examples/hello_world.bude -a -o hello_world.asm
$ fasm hello_world.asm
flat assembler  version 1.73.31  (1048576 kilobytes memory)
3 passes, 0.1 seconds, 2560 bytes.
$ ./hello_world.exe
Hello, World!
```

This assumes FASM has been installed and is in the PATH variable. Note that the
actual output of FASM may vary.

## Language Features

Bude has a stack for storing 64-bit words. There are instructions to manipulate the stack.

Notation: below, different annotations are used to describe how data on the stack should be
interpreted for the differet instructions. Note that regardless of the size of the underlying
C type, these values always take up one full stack slot (64 bits). A C-like language is used
for the result expressions of some operations.

Data:
* _w_ &ndash; arbitrary stack word
* _i_ &ndash; signed integer value
* _p_ &ndash; pointer value
* _c_ &ndash; UTF-8 codepoint
* `<literal>` &ndash; the literal value denoted
* _pk_ &ndash; structural "pack" type
* _cmp_ &ndash; structural "comp" type
* _T_ &ndash; type variable "T"

Syntax:
* <_symbol_> &ndash; syntactically required symbol
* _section_ &ndash; syntactical section
* [ _optional-section_ ] &ndash; optional syntactic section
* &hellip; &ndash; syntax can be repeated arbitrarily
* ( _groups_ ) &ndash; group syntax sections
* _choice1_|_choice2_ &ndash; syntax option

### Push instructions

`"Lorem ipsum"` &rarr; _p_ _i_ : Push the specified string to the stack
along with its length.

`'q'` &rarr; _c_ : Push the specified character to the stack.

`42` &rarr; _i_ : Push the specified integer to the stack.

### Pop instructions

_w_ `pop` &rarr; &varnothing; : Pop the top element from the stack and discard it.

_i_ `print` &rarr; &varnothing; : Pop the top element from the stack and print it as an integer.

_c_ `print-char` &rarr; &varnothing; : Pop the top element from the stack and print it as a
unicode character.

### Field access operations

_pk_ <_field-name_: _T_> &rarr; _pk_ _T_ : Push the specified field from the pack.

_pk_ _T_ `<-` <_field-name_: _T_> &rarr; _pk_ : Pop the top stack value and use it to set the
specified field of the pack underneath.

_cmp_ <_field-name_: _T_> &rarr; _cmp_ _T_ : Push the specified field from the comp.

_cmp_ _T_ `<-` <_field-name_: _T_> &rarr; _cmp_ : Pop the top stack value and use it to set the
specified field of the comp underneath.

### Arithmetic operations

_i1_ _i2_ `+` &rarr; (_i1_ + _i2_) : Pop the top two elements and push their sum.

_i1_ _i2_ `-` &rarr; (_i1_ - _i2_) : Pop the top two elements and push their difference.

_i1_ _i2_ `*` &rarr; (_i1_ \* _i2_) : Pop the top two elements and push their product.

_i1_ _i2_ `divmod` &rarr; (_i1_ / _i2_) (_i1_ \% _i2_) : Pop the top two elements and push the
quotient and remainder from their division. The remainder is always non-negative
(Euclidean division).

_i1_ _i2_ `idivmod` &rarr; (_i1_ /<sub>trunc</sub> _i2_) (_i1_ \%<sub>trunc</sub> _i2_) :
Pop the top two elements
and push the quotient and remainder of their truncated divison. The quotient is rounded
towards zero and the remainder can be negative.

_i1_ _i2_ `edivmod` &rarr; (_i1_ /<sub>euclid</sub> _i2_) (_i1_ \%<sub>euclid</sub> _i2_) :
Pop the top two elements and
push the quotient and remainer of their Euclidean division. The quotient is rounded towards
negative infinity and the remainer is always non-negative.

_i1_ _i2_ `/` &rarr; (_i1_ / _i2_) : Pop the top two stack elements and push the quotient from
their division. Acts like `divmod pop` to pop the remainder.

_i1_ _i2_ `%` &rarr; (_i1_ / _i2_) : Pop the top two stack elements and push the remainder from
their division. Acts like `divmod swap pop` to pop the quotient.

### Comparison operations

_i1_ _i2_ `<` &rarr; (_i1_ < _i2_) : Pop _i2_ and _i1_ and push back 1 if _i1_ < _i2_
or 0 if not.

_i1_ _i2_ `<=` &rarr; (_i1_ <= _i2_) : Pop _i2_ and _i1_ and push back 1 if  _i1_ &le; _i2_
or 0 if not.

_i1_ _i2_ `=` &rarr; (_i1_ == _i2_) : Pop _i2_ and _i1_ and push back 1 if _i1_ = _i2_
or 0 if not.

_i1_ _i2_ `>=` &rarr; (_i1_ >= _i2_) : Pop _i2_ and _i1_ and push back 1 if _i1_ &ge; _i2_
or 0 if not.

_i1_ _i2_ `>` &rarr; (_i1_ > _i2_) : Pop _i2_ and _i1_ and push back 1 if _i1_ > _i2_
or 0 if not.

_i1_ _i2_ `/=` &rarr; (_i1_ != _i2_) : Pop _i2_ and _i1_ and push back 1 if _i1_ &ne; _i2_
or 0 if not.

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

### Constructors

_F1_ _F2_ _F3_ &hellip; <_pack-name_> &rarr; _pk_ : Construct a pack with the field types
_F1_, _F2_, _F3_, &hellip;.

_F1_ _F2_ _F3_ &hellip; <_comp-name_> &rarr; _pk_ : Construct a comp with the field types
_F1_, _F2_, _F3_, &hellip;.

### Destructors

_pk_ `unpack` &rarr; _F1_ _F2_ _F3_ &hellip; : Unpack the pack on the top of the stack into its
fields with types _F1_, _F2_, _F3_, &hellip;.

_cmp_ `decomp` &rarr; _F1_ _F2_ _F3_ &hellip; : Decompose the comp on the top of the stack into
its fields with types _F1_, _F2_, _F3_, &hellip;.

### Control flow constructs

`if` _condition_ `then` _then-body_ [`elif` _elif-condition_ `then` _elif-then-body_ &hellip;]
[`else` _else-body_] `end`

`while` _condition_ `do` _body_ `end`

`for` [<_loop-var_> (`to`|`from`)] _count_ `do` _body_ `end`

The `for` loop has two forms. The simple form (`for` _count_ &hellip;) loops the number of
times specified by _count_. The counting form (`for` <_loop-var_> `to` _count_ &hellip;,
`for` <_loop-var_> `from` _count_ &hellip;) creates a loop variable and binds it to the name
specified, which can be accessed inside the loop. The value stored in the loop variable either
starts at zero and counts up `to` _count_ or counts down to zero `from` _count_.

_P1_ _P2_ _P3_ &hellip; <_func-name_> &rarr; _R1_ _R2_ _R3_ : Call the specified function which
takes parameters with types _P1_, _P2_, _P3_, &hellip; and returns values with types _R1_, _R2_,
_R3_.

`ret` : Return from the current function.

### Definitions

`pack` <_pack-name_> `def` (<_field-name_> `->` <_field-type_>) &hellip; `end`

`comp` <_comp-name_> `def` (<_field-name_> `->` <_field-type_>) &hellip; `end`

`func` <_param-type_> &hellip; <_func-name_> [`->` <_ret-type_> &hellip;] _func-body_ `end`

