#ifndef INTERPRETER_H
#define INTERPRETER_H

enum interpret_result {
    INTERPRET_OK,
    INTERPRET_ERROR,
};

enum interpret_result interpret(void);

#endif
