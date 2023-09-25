#ifndef GENERATOR_H
#define GENERATOR_H

enum generate_result {
    GENERATE_OK,
    GENERATE_ERROR,
};

enum generate_result generate(struct ir_block *block);

#endif
