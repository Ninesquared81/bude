"Module containing definitions for Bude's Word-oriented IR format."

from __future__ import annotations

import enum
from typing import Iterator


class Opcode(enum.IntEnum):
    """An enumeration representing the opcodes of the word-oriented IR."""
    # NOTE: It is important to keep this in agreement with `enum w_opcode` defined in ../src/ir.h.
    NOP = 0  # Explicitly set since enum.auto() starts at 1 by default
    PUSH8 = enum.auto()
    PUSH16 = enum.auto()
    PUSH32 = enum.auto()
    PUSH64 = enum.auto()
    PUSH_INT8 = enum.auto()
    PUSH_INT16 = enum.auto()
    PUSH_INT32 = enum.auto()
    PUSH_INT64 = enum.auto()
    PUSH_CHAR8 = enum.auto()
    PUSH_CHAR16 = enum.auto()
    PUSH_CHAR32 = enum.auto()
    LOAD_STRING8 = enum.auto()
    LOAD_STRING16 = enum.auto()
    LOAD_STRING32 = enum.auto()
    POP = enum.auto()
    POPN8 = enum.auto()
    POPN16 = enum.auto()
    POPN32 = enum.auto()
    ADD = enum.auto()
    AND = enum.auto()
    DEREF = enum.auto()
    DIVMOD = enum.auto()
    IDIVMOD = enum.auto()
    EDIVMOD = enum.auto()
    DUPE = enum.auto()
    DUPEN8 = enum.auto()
    DUPEN16 = enum.auto()
    DUPEN32 = enum.auto()
    EQUALS = enum.auto()
    EXIT = enum.auto()
    FOR_DEC_START = enum.auto()
    FOR_DEC = enum.auto()
    FOR_INC_START = enum.auto()
    FOR_INC = enum.auto()
    GET_LOOP_VAR = enum.auto()
    GREATER_EQUALS = enum.auto()
    GREATER_THAN = enum.auto()
    HIGHER_SAME = enum.auto()
    HIGHER_THAN = enum.auto()
    JUMP = enum.auto()
    JUMP_COND = enum.auto()
    JUMP_NCOND = enum.auto()
    LESS_EQUALS = enum.auto()
    LESS_THAN = enum.auto()
    LOWER_SAME = enum.auto()
    LOWER_THAN = enum.auto()
    MULT = enum.auto()
    NOT = enum.auto()
    NOT_EQUALS = enum.auto()
    OR = enum.auto()
    PRINT = enum.auto()
    PRINT_CHAR = enum.auto()
    PRINT_INT = enum.auto()
    PRINT_STRING = enum.auto()
    SUB = enum.auto()
    SWAP = enum.auto()
    SWAP_COMPS8 = enum.auto()
    SWAP_COMPS16 = enum.auto()
    SWAP_COMPS32 = enum.auto()
    SX8 = enum.auto()
    SX8L = enum.auto()
    SX16 = enum.auto()
    SX16L = enum.auto()
    SX32 = enum.auto()
    SX32L = enum.auto()
    ZX8 = enum.auto()
    ZX8L = enum.auto()
    ZX16 = enum.auto()
    ZX16L = enum.auto()
    ZX32 = enum.auto()
    ZX32L = enum.auto()
    PACK1 = enum.auto()
    PACK2 = enum.auto()
    PACK3 = enum.auto()
    PACK4 = enum.auto()
    PACK5 = enum.auto()
    PACK6 = enum.auto()
    PACK7 = enum.auto()
    PACK8 = enum.auto()
    UNPACK1 = enum.auto()
    UNPACK2 = enum.auto()
    UNPACK3 = enum.auto()
    UNPACK4 = enum.auto()
    UNPACK5 = enum.auto()
    UNPACK6 = enum.auto()
    UNPACK7 = enum.auto()
    UNPACK8 = enum.auto()
    PACK_FIELD_GET = enum.auto()
    COMP_FIELD_GET8 = enum.auto()
    COMP_FIELD_GET16 = enum.auto()
    COMP_FIELD_GET32 = enum.auto()
    PACK_FIELD_SET = enum.auto()
    COMP_FIELD_SET8 = enum.auto()
    COMP_FIELD_SET16 = enum.auto()
    COMP_FIELD_SET32 = enum.auto()
    COMP_SUBCOMP_GET8 = enum.auto()
    COMP_SUBCOMP_GET16 = enum.auto()
    COMP_SUBCOMP_GET32 = enum.auto()
    COMP_SUBCOMP_SET8 = enum.auto()
    COMP_SUBCOMP_SET16 = enum.auto()
    COMP_SUBCOMP_SET32 = enum.auto()
    CALL8 = enum.auto()
    CALL16 = enum.auto()
    CALL32 = enum.auto()
    RET = enum.auto()


class Instruction:
    def __init__(self, op: Opcode, *operands: int) -> None:
        self.op = op
        self.operands = operands

    def __repr__(self) -> str:
        return f"{type(self)}({', '.join(map(repr, (self.op, *self.operands)))})"

    def __str__(self) -> str:
        return f"({' '.join((self.op.name, *map(str, self.operands)))})"


class Block:
    def __init__(self, code: bytes) -> None:
        self.code = code

    def __iter__(self) -> Iterator[tuple[ip, Instruction]]:
        ip = 0
        while ip < len(self.code):
            ip, instruction = self.decode(ip)
            yield instruction

    def read_u8(self, ip) -> tuple[int, int]:
        return ip + 1, int.from_bytes(self.code[ip:ip+1], "little")

    def read_u16(self, ip) -> tuple[int, int]:
        return ip + 2, int.from_bytes(self.code[ip:ip+2], "little")

    def read_u32(self, ip) -> tuple[int, int]:
        return ip + 4, int.from_bytes(self.code[ip:ip+4], "little")

    def read_u64(self, ip) -> tuple[int, int]:
        return ip + 8, int.from_bytes(self.code[ip:ip+8], "little")

    def read_s8(self, ip) -> tuple[int, int]:
        return ip + 1, int.from_bytes(self.code[ip:ip+1], "little", signed=True)

    def read_s16(self, ip) -> tuple[int, int]:
        return ip + 2, int.from_bytes(self.code[ip:ip+2], "little", signed=True)

    def read_s32(self, ip) -> tuple[int, int]:
        return ip + 4, int.from_bytes(self.code[ip:ip+4], "little", signed=True)

    def read_s64(self, ip) -> tuple[int, int]:
        return ip + 8, int.from_bytes(self.code[ip:ip+8], "little", signed=True)

    def decode(self, ip) -> tuple[int, Instruction]:
        op = Opcode(self.code[ip])
        operand_readers = self.INSTRUCTION_TABLE[op]
        operands = []
        ip += 1
        for opreader in operand_readers:
            ip, operand = opreader(self, ip)
            operands.append(operand)
        return ip, Instruction(op, *operands)

    INSTRUCTION_TABLE = {
        Opcode.NOP:                (),
        Opcode.PUSH8:              (read_u8, ),
        Opcode.PUSH16:             (read_u16,),
        Opcode.PUSH32:             (read_u32,),
        Opcode.PUSH64:             (read_u64,),
        Opcode.PUSH_INT8:          (read_s8, ),
        Opcode.PUSH_INT16:         (read_s16,),
        Opcode.PUSH_INT32:         (read_s32,),
        Opcode.PUSH_INT64:         (read_s64,),
        Opcode.PUSH_CHAR8:         (read_u8, ),
        Opcode.PUSH_CHAR16:        (read_u16,),
        Opcode.PUSH_CHAR32:        (read_u32,),
        Opcode.LOAD_STRING8:       (read_u8, ),
        Opcode.LOAD_STRING16:      (read_u16,),
        Opcode.LOAD_STRING32:      (read_u32,),
        Opcode.POP:                (),
        Opcode.POPN8:              (read_s8, ),
        Opcode.POPN16:             (read_s16,),
        Opcode.POPN32:             (read_s32,),
        Opcode.ADD:                (),
        Opcode.AND:                (),
        Opcode.DEREF:              (),
        Opcode.DIVMOD:             (),
        Opcode.IDIVMOD:            (),
        Opcode.EDIVMOD:            (),
        Opcode.DUPE:               (),
        Opcode.DUPEN8:             (read_s8, ),
        Opcode.DUPEN16:            (read_s16,),
        Opcode.DUPEN32:            (read_s32,),
        Opcode.EQUALS:             (),
        Opcode.EXIT:               (),
        Opcode.FOR_DEC_START:      (read_s16,),
        Opcode.FOR_DEC:            (read_s16,),
        Opcode.FOR_INC_START:      (read_s16,),
        Opcode.FOR_INC:            (read_s16,),
        Opcode.GET_LOOP_VAR:       (read_u16,),
        Opcode.GREATER_EQUALS:     (),
        Opcode.GREATER_THAN:       (),
        Opcode.HIGHER_SAME:        (),
        Opcode.HIGHER_THAN:        (),
        Opcode.JUMP:               (read_s16,),
        Opcode.JUMP_COND:          (read_s16,),
        Opcode.JUMP_NCOND:         (read_s16,),
        Opcode.LESS_EQUALS:        (),
        Opcode.LESS_THAN:          (),
        Opcode.LOWER_SAME:         (),
        Opcode.LOWER_THAN:         (),
        Opcode.MULT:               (),
        Opcode.NOT:                (),
        Opcode.NOT_EQUALS:         (),
        Opcode.OR:                 (),
        Opcode.PRINT:              (),
        Opcode.PRINT_CHAR:         (),
        Opcode.PRINT_INT:          (),
        Opcode.PRINT_STRING:       (),
        Opcode.SUB:                (),
        Opcode.SWAP:               (),
        Opcode.SWAP_COMPS8:        (read_s8,  read_s8, ),
        Opcode.SWAP_COMPS16:       (read_s16, read_s16,),
        Opcode.SWAP_COMPS32:       (read_s32, read_s32,),
        Opcode.SX8:                (),
        Opcode.SX8L:               (),
        Opcode.SX16:               (),
        Opcode.SX16L:              (),
        Opcode.SX32:               (),
        Opcode.SX32L:              (),
        Opcode.ZX8:                (),
        Opcode.ZX8L:               (),
        Opcode.ZX16:               (),
        Opcode.ZX16L:              (),
        Opcode.ZX32:               (),
        Opcode.ZX32L:              (),
        Opcode.PACK1:              (read_u8,) * 1,
        Opcode.PACK2:              (read_u8,) * 2,
        Opcode.PACK3:              (read_u8,) * 3,
        Opcode.PACK4:              (read_u8,) * 4,
        Opcode.PACK5:              (read_u8,) * 5,
        Opcode.PACK6:              (read_u8,) * 6,
        Opcode.PACK7:              (read_u8,) * 7,
        Opcode.PACK8:              (read_u8,) * 8,
        Opcode.UNPACK1:            (read_u8,) * 1,
        Opcode.UNPACK2:            (read_u8,) * 2,
        Opcode.UNPACK3:            (read_u8,) * 3,
        Opcode.UNPACK4:            (read_u8,) * 4,
        Opcode.UNPACK5:            (read_u8,) * 5,
        Opcode.UNPACK6:            (read_u8,) * 6,
        Opcode.UNPACK7:            (read_u8,) * 7,
        Opcode.UNPACK8:            (read_u8,) * 8,
        Opcode.PACK_FIELD_GET:     (read_u8, ),
        Opcode.COMP_FIELD_GET8:    (read_u8, ),
        Opcode.COMP_FIELD_GET16:   (read_u16,),
        Opcode.COMP_FIELD_GET32:   (read_u32,),
        Opcode.PACK_FIELD_SET:     (read_u8, ),
        Opcode.COMP_FIELD_SET8:    (read_u8, ),
        Opcode.COMP_FIELD_SET16:   (read_u16,),
        Opcode.COMP_FIELD_SET32:   (read_u32,),
        Opcode.COMP_SUBCOMP_GET8:  (read_u8,  read_u8, ),
        Opcode.COMP_SUBCOMP_GET16: (read_u16, read_u16,),
        Opcode.COMP_SUBCOMP_GET32: (read_u32, read_u32,),
        Opcode.COMP_SUBCOMP_SET8:  (read_u8,  read_u8, ),
        Opcode.COMP_SUBCOMP_SET16: (read_u16, read_u16,),
        Opcode.COMP_SUBCOMP_SET32: (read_u32, read_u32,),
        Opcode.CALL8:              (read_u8, ),
        Opcode.CALL16:             (read_u16,),
        Opcode.CALL32:             (read_u32,),
        Opcode.RET:                (),
    }
