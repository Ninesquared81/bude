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

## Language Overview

Bude has a stack for storing 64-bit words. There are instructions to manipulate the stack.

Notation: below, different annotations are used to describe how data on the stack should be
interpreted for the differet instructions. Note that regardless of the size of the underlying
C type, these values always take up one full stack slot (64 bits). A C-like language is used
for the result expressions of some operations.

Data:

* _w_ &ndash; arbitrary stack word
* _i_ &ndash; signed integer value
* _f_ &ndash; floating-point value (either 32- or 64-bit)
* _n_ &ndash; number value (integer or floating-point)
* _p_ &ndash; pointer value
* _b_ &ndash; Boolean value
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

`"Lorem ipsum"` &rarr; ( _p_<sub>start</sub> _i_<sub>length</sub> ) :
Push the specified string to the stack.

`'q'` &rarr; _c_ : Push the specified character to the stack.

`42` &rarr; _i_ : Push the specified integer to the stack.

`true` &rarr; _b_ : Push the Boolean "true" value to the stack.

`false` &rarr; _b_ : Push the Boolean "false" value to the stack.

### Pop instructions

_w_ `pop` &rarr; &varnothing; : Pop the top element from the stack and discard it.

_w_ `print` &rarr; &varnothing; : Pop the top element from the stack and print it in a format
based on its type.

_c_ `print-char` &rarr; &varnothing; : Pop the top element from the stack and print it as a
unicode character.

### Field access operations

_pk_ <_field-name_: _T_> &rarr; _pk_ _T_ :
Push the specified field from the pack.

_pk_ _T_ `<-` <_field-name_: _T_> &rarr; _pk_ :
Pop the top stack value and use it to set the specified field of the pack underneath.

_cmp_ <_field-name_: _T_> &rarr; _cmp_ _T_ :
Push the specified field from the comp.

_cmp_ _T_ `<-` <_field-name_: _T_> &rarr; _cmp_ :
Pop the top stack value and use it to set the specified field of the comp underneath.

### Arithmetic operations

_n<sub>1</sub>_ _n<sub>2</sub>_ `+` &rarr;
(_n<sub>1</sub>_ + _n<sub>2</sub>_) :
Pop the top two elements and push their sum.

_n<sub>1</sub>_ _n<sub>2</sub>_ `-` &rarr; (_n<sub>1</sub>_ - _n<sub>2</sub>_) :
Pop the top two elements and push their difference.

_n<sub>1</sub>_ _n<sub>2</sub>_ `*` &rarr;
(_n<sub>1</sub>_ \* _n<sub>2</sub>_) :
Pop the top two elements and push their product.

_i<sub>1</sub>_ _i<sub>2</sub>_ `divmod` &rarr;
(_i<sub>1</sub>_ / _i<sub>2</sub>_)
(_i<sub>1</sub>_ \% _i<sub>2</sub>_) :
Pop the top two elements and push the quotient and remainder from their division.
The remainder is always non-negative (Euclidean division).

_i<sub>1</sub>_ _i<sub>2</sub>_ `idivmod` &rarr;
(_i<sub>1</sub>_ /<sub>trunc</sub> _i<sub>2</sub>_)
(_i<sub>1</sub>_ \%<sub>trunc</sub> _i<sub>2</sub>_) :
Pop the top two elements and push the quotient and remainder of their truncated divison.
The quotient is rounded towards zero and the remainder can be negative.

_i<sub>1</sub>_ _i<sub>2</sub>_ `edivmod` &rarr;
(_i<sub>1</sub>_ /<sub>euclid</sub> _i<sub>2</sub>_)
(_i<sub>1</sub>_ \%<sub>euclid</sub> _i<sub>2</sub>_) :
Pop the top two elements and push the quotient and remainder of their Euclidean division.
The quotient is rounded towards negative infinity and the remainder is always non-negative.

_i<sub>1</sub>_ _i<sub>2</sub>_ `/` &rarr; (_i<sub>1</sub>_ / _i<sub>2</sub>_) :
Pop the top two stack elements and push the quotient from their division.
Acts like `divmod pop` to pop the remainder.

_f<sub>1</sub>_ _f<sub>2</sub>_ `/` &rarr; (_f<sub>1</sub>_ / _f<sub>2</sub>_) :
Pop the top two floating-point stack elements and push their ratio. Unlike the integer version,
this is exact division and thus has no remainder.

_i<sub>1</sub>_ _i<sub>2</sub>_ `%` &rarr; (_i<sub>1</sub>_ \% _i<sub>2</sub>_) :
Pop the top two stack elements and push the remainder from their division.
Acts like `divmod swap pop` to pop the quotient.

_n_ `~` &rarr; (-_n_) :
Negate the top stack element.

### Comparison operations

_n<sub>1</sub>_ _n<sub>2</sub>_ `<` &rarr; (_n<sub>1</sub>_ < _n<sub>2</sub>_) :
Pop _n<sub>2</sub>_ and _n<sub>1</sub>_ and push back whether
_n<sub>1</sub>_ < _n<sub>2</sub>_.

_n<sub>1</sub>_ _n<sub>2</sub>_ `<=` &rarr; (_n<sub>1</sub>_ <= _n<sub>2</sub>_) :
Pop _n<sub>2</sub>_ and _n<sub>1</sub>_ and push back whether
_n<sub>1</sub>_ &le; _n<sub>2</sub>_.

_n<sub>1</sub>_ _n<sub>2</sub>_ `=` &rarr; (_n<sub>1</sub>_ == _n<sub>2</sub>_) :
Pop _n<sub>2</sub>_ and _n<sub>1</sub>_ and push back whether
_n<sub>1</sub>_ = _n<sub>2</sub>_.

_n<sub>1</sub>_ _n<sub>2</sub>_ `>=` &rarr; (_n<sub>1</sub>_ >= _n<sub>2</sub>_) :
Pop _n<sub>2</sub>_ and _n<sub>1</sub>_ and push back whether
_n<sub>1</sub>_ &ge; _n<sub>2</sub>_.

_n<sub>1</sub>_ _n<sub>2</sub>_ `>` &rarr; (_n<sub>1</sub>_ > _n<sub>2</sub>_) :
Pop _n<sub>2</sub>_ and _n<sub>1</sub>_ and push back whether
_n<sub>1</sub>_ > _n<sub>2</sub>_.

_n<sub>1</sub>_ _n<sub>2</sub>_ `/=` &rarr; (_n<sub>1</sub>_ != _n<sub>2</sub>_) :
Pop _n<sub>2</sub>_ and _n<sub>1</sub>_ and push back whether
_n<sub>1</sub>_ &ne; _n<sub>2</sub>_.

### Logical operations

_w_ `not` &rarr; !_w_ :
Replace the top element with its logical inverse (i.e. non-zero &rarr; `false`, 0 &rarr; `true`).

_w<sub>1</sub>_ _w<sub>2</sub>_ `or` &rarr; (_w<sub>1</sub>_ or _w<sub>2</sub>_) :
Drop _w<sub>1</sub>_ if it is equal to zero, else drop _w<sub>2</sub>_.

_w<sub>1</sub>_ _w<sub>2</sub>_ `and` &rarr; (_w<sub>1</sub>_ and _w<sub>2</sub>_) :
Drop _w<sub>1</sub>_ if it is non-zero, else drop _w<sub>2</sub>_.

### Memory operations

_p_ `deref` &rarr; (byte \*_p_) : Pop the pointer _p_ and push the first byte it points to.

<_var-name_: _T_> &rarr; _T_ : Push the specified local variable.

_T_ `<-` <_var-name_: _T_> &rarr; &varnothing; : Pop the top stack value and use it to set the
specified local variable.

### Stack manipulation

_w<sub>1</sub>_ _w<sub>2</sub>_ `swap` &rarr; _w<sub>2</sub>_ _w<sub>1</sub>_ :
Swap the top two elements on the stack.

_w<sub>1</sub>_ `dupe` &rarr; _w<sub>1</sub>_ _w<sub>1</sub>_ :
Duplicate the top element on the stack.

_w<sub>1</sub>_ _w<sub>2</sub>_ `over` &rarr; _w<sub>1</sub>_ _w<sub>2</sub>_ _w<sub>1</sub>_ :
Copy the next element over the top element.

_w<sub>1</sub>_ _w<sub>2</sub>_ _w<sub>3</sub>_ `rot` &rarr;
_w<sub>2</sub>_ _w<sub>3</sub>_ _w<sub>1</sub>_ :
Rotate the top three stack elements.

### Conversions

_w_ `to` <_type_: _T_> &rarr; _T_ : Convert top stack element to type _T_ (value-preserving).

_w_ `as` <_type_: _T_> &rarr; _T_ : Coerce top stack element to type _T_
(bit-pattern--preserving, with truncation).

### Constructors

_F<sub>1</sub>_ _F<sub>2</sub>_ _F<sub>3</sub>_ &hellip; <_pack-name_> &rarr; _pk_ :
Construct a pack with the field types
_F<sub>1</sub>_, _F<sub>2</sub>_, _F<sub>3</sub>_, &hellip;.

_F<sub>1</sub>_ _F<sub>2</sub>_ _F<sub>3</sub>_ &hellip; <_comp-name_> &rarr; _pk_ :
Construct a comp with the field types
_F<sub>1</sub>_, _F<sub>2</sub>_, _F<sub>3</sub>_, &hellip;.

### Destructors

_pk_ `unpack` &rarr; _F<sub>1</sub>_ _F<sub>2</sub>_ _F<sub>3</sub>_ &hellip; :
Unpack the pack on the top of the stack into its fields with types
_F<sub>1</sub>_, _F<sub>2</sub>_, _F<sub>3</sub>_, &hellip;.

_cmp_ `decomp` &rarr; _F<sub>1</sub>_ _F<sub>2</sub>_ _F<sub>3</sub>_ &hellip; :
Decompose the comp on the top of the stack into its fields with types
_F<sub>1</sub>_, _F<sub>2</sub>_, _F<sub>3</sub>_, &hellip;.

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

_P<sub>1</sub>_ _P<sub>2</sub>_ _P<sub>3</sub>_ &hellip; <_func-name_> &rarr;
_R<sub>1</sub>_ _R<sub>2</sub>_ _R<sub>3</sub>_ :
Call the specified function which takes parameters with types
_P<sub>1</sub>_, _P<sub>2</sub>_, _P<sub>3</sub>_, &hellip; and returns values with types
_R<sub>1</sub>_, _R<sub>2</sub>_, _R<sub>3</sub>_.

`ret` : Return from the current function.

### Definitions

`pack` <_pack-name_> `def` (<_field-name_> `->` <_field-type_>) &hellip; `end`

`comp` <_comp-name_> `def` (<_field-name_> `->` <_field-type_>) &hellip; `end`

`func` <_param-type_> &hellip; <_func-name_> [`->` <_ret-type_> &hellip;] _func-body_ `end`

`var` <_var-name_> `->` <_var-type_> &hellip; `end`

`import` <_lib-name_> `def`
(`func` <_param-type_> &hellip; <_ext-func-name_> [`->` <_ret-type_> &hellip;]
[`from` <_alias_>] [`with` <_call-conv_>] `end`)
&hellip; `end`

## Language Features

### Packs and Comps

Bude stores data on the stack. Each value fits into one 64-bit stack slot.
This makes things easier to work with, but what if we wanted to compose many "things" together
while still treating it as one unit. We need some sorty of structural type, like `struct` in C.

Luckily, Bude has us covered with not one, but two structural types. The first of these is the
_pack_, which still takes up one stack slot but can have multiple non-overlapping fields inside.
Each field is accessed by a corresponding name and can hold a value of a certain type.
The syntax for defining a pack type is outlined above, but to give an example, let's say we want
to store an RGBA value on the stack. Each channel is in the range 0-255, so 4 channels would
only need 32 bits in total &ndash; much smaller than a stack slot. This is a good call
for a pack. We define our pack as:

```
pack RGBA-Colour def
    r -> u8
    g -> u8
    b -> u8
    a -> u8
end
```
Now we can construct an `RGBA-Colour` value using the symbol "`RGBA-Colour`" as a constructor:

```
70u8 130u8 180u8 255u8 RGBA-Colour  # Creates an RGBA colour (70, 130, 180, 255) (HTML SteelBlue)
```
We can extract a field from the newly-created pack by using its name as a getter:
```
g print  # prints 130
```
We can also set a field using the `<-` operator:
```
150 <- a
a print   # prints 150
```
We can deconstruct a pack to release all its fields using the `unpack` instruction:
```
unpack
print  # prints 150
print  # prints 180
print  # prints 130
print  # prints 70
```

Packs are great and all, but what if we wanted to store, say, a 3-D vector? If we used 32-bit
floats, we'd fill up our _pack_ after only 2 components, and for 64-bit floats, we'd fill it up
after only one component. Luckily, Bude has another structural type available to us: the _comp_.
_Comps_ allow us to _compose_ several stack words into a single unit. Syntactically, they work
very similarly to _packs_. For our vector example:
```
comp Vec3D def
    x -> f64
    y -> f64
    z -> f64
end
```
We, again, have named fields. This time, each field correpsonds to a different stack slot within
the comp. Like with _packs_, we can access fields by their name:
```
5.0 42.0 -0.5 Vec3D
x print  # prints 5.0
y 5 - <- y
y print  # prints 37.0
```
As with _packs_, we have an instruction to deconstruct a comp. This time, it's called `decomp`
(for decompose):
```
decomp
print  # prints -0.5
print  # prints 37.0
print  # prints 5.0
```
### Functions

Most programming languages have a concept of a function or procedure: a block of code that
can be executed on demand and return to the callsite. Bude has functions, too. For an example,
a function `hello` which prints the string `"Hello, World!"`:

```
func hello def
    "Hello, World!\n" print
end
```
The function is introduced by the keyword `func`. Then, we have the function's name followed by
the keyword `def`, which marks the start of the function's body. This is then terminated by `end`,
which is also where the function definition ends.

Often, we want to transfer data to/from a function through parameters and return values. In Bude,
arguments and return values are passed/left on the stack. Unlike functions in most programming
languages, Bude functions can return multiple values. It's as simple as leaving multiple values
on the stack at the end of the function. If we want our Bude function to receive parameters or
leave return values, we must specify this with a function _signature_. The signature comes
between the `func` and `def` delimiters and is an expansion on the simple name we used in the
example above (in fact, in that example, "`hello`" _is_ the signature). The syntax for signatures
is best illustrated with an example:

```
func int int diff-squares -> int
    dupe * swap
    dupe * swap
    -
end
```
This function returns the difference of the squares of the two integers passed. The signature
starts with the parameter types (if any), followed by the function name. Return types are
introduced by a right arrow `->` following the name. If there are no return types, the arrow is
omitted. We can have multiple return values:
```
func first-five-primes -> int int int int int def
    2 3 5 7 11
end
```
