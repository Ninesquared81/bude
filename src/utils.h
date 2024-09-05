#ifndef UTILS_H
#define UTILS_H

// General macros. Useful when working with x-macros

/* ID -- return argument identically. */
#define ID(x) x
/* ID_LIST -- return argument identically as part of an initialiser list. */
#define ID_LIST(x) x,
/* STRINGIFY -- turn token into a string literal. */
#define STRINGIFY(x) #x
/* STRING_TABLE -- turn token into designated initialiser associating it with its string value. */
#define STRING_TABLE(x) [x] = #x,

#endif
