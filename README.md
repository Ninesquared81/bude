# Bude
Bude is a stack-based language inspired in part by [Porth](https://gitlab.com/tsoding/porth).

NOTE: The language is currently very unfinished.

## Language Features

Bude has a stack for storing 64-bit words. There are instructions to manipulate the stack.

### Push instructions

`"Lorem ipsum"` &rarr; _p_ _i_ : Pushes the specified string to the stack
along with its length.

`42` &rarr; _i_ : Pushes the specified integer to the stack.

### Pop instructions

_w_ `pop` &rarr; &varnothing; : Pops the top element from the stack and discards it.

### Arithmetic operations

_i1_ _i2_ `+` &rarr; (_i1_ + _i2_) : Pops the top two elements and pushes their sum.

_i1_ _i2_ `-` &rarr; (_i1_ - _i2_) : Pops the top two elements and pushes their difference.

_i1_ _i2_ `*` &rarr; (_i1_ \* _i2_) : Pops the top two elements and pushes their product.

_i1_ _i2_ `/%` &rarr; (_i1_ / _i2_) (_i1_ \% _i2_) : Pops the top two elements and pushes their
quotient and remainder.

### Stack manipulation

_w1_ _w2_ `swap` &rarr; _w2_ _w1_ : Swap the top two elements on the stack.

_w1_ `dupe` &rarr; _w1_ _w1_ : Duplicate the top element on the stack.

### Conrol flow constructs

`if` _condition_ `then` _then-body_ [`elif` _elif-condition_ `then` _elif-then-body_ &hellip;]
[`else` _else-body_] `end`

`while` _condition_ `do` _body_ `end`

