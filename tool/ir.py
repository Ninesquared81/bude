"Module containing definitions for Bude's Word-oriented IR format."

from __future__ import annotations

import abc
import dataclasses
import enum
import sys
from typing import Iterator


BUILTIN_TYPE_COUNT = 17


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
    PUSH_FLOAT32 = enum.auto()
    PUSH_FLOAT64 = enum.auto()
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
    ADDF32 = enum.auto()
    ADDF64 = enum.auto()
    AND = enum.auto()
    DEREF = enum.auto()
    DIVF32 = enum.auto()
    DIVF64 = enum.auto()
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
    LOCAL_GET = enum.auto()
    LOCAL_SET = enum.auto()
    LOWER_SAME = enum.auto()
    LOWER_THAN = enum.auto()
    MULT = enum.auto()
    MULTF32 = enum.auto()
    MULTF64 = enum.auto()
    NOT = enum.auto()
    NOT_EQUALS = enum.auto()
    OR = enum.auto()
    PRINT = enum.auto()
    PRINT_CHAR = enum.auto()
    PRINT_FLOAT = enum.auto()
    PRINT_INT = enum.auto()
    PRINT_STRING = enum.auto()
    SUB = enum.auto()
    SUBF32 = enum.auto()
    SUBF64 = enum.auto()
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
    FPROM = enum.auto()
    FPROML = enum.auto()
    FDEM = enum.auto()
    ICONVF32 = enum.auto()
    ICONVF32L = enum.auto()
    ICONVF64 = enum.auto()
    ICONVF64L = enum.auto()
    FCONVI32 = enum.auto()
    FCONVI64 = enum.auto()
    ICONVC32 = enum.auto()
    CHAR_8CONV32 = enum.auto()
    CHAR_32CONV8 = enum.auto()
    CHAR_16CONV32 = enum.auto()
    CHAR_32CONV16 = enum.auto()
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
    EXTCALL8 = enum.auto()
    EXTCALL16 = enum.auto()
    EXTCALL32 = enum.auto()
    RET = enum.auto()


class Operand(abc.ABC, int):
    """Abstract base class for operands with a type and fixed size."""
    def to_bytes(self, /, byteorder="big") -> bytes:
        return super().to_bytes(length=self.size(), byteorder=byteorder, signed=self.issigned())

    @classmethod
    def from_bytes(cls, src: bytes, byteorder="big") -> bytes:
        return super().from_bytes(src, byteorder=byteorder, signed=cls.issigned())

    @staticmethod
    @abc.abstractmethod
    def size() -> int:
        """Return the size (in bytes) of the operand."""

    @staticmethod
    @abc.abstractmethod
    def issigned() -> bool:
        """Return whether the operand is signed."""


class U8(Operand):
    @staticmethod
    def size() -> int:
        return 1

    @staticmethod
    def issigned() -> int:
        return False


class U16(Operand):
    @staticmethod
    def size() -> int:
        return 2

    @staticmethod
    def issigned() -> int:
        return False


class U32(Operand):
    @staticmethod
    def size() -> int:
        return 4

    @staticmethod
    def issigned() -> int:
        return False


class U64(Operand):
    @staticmethod
    def size() -> int:
        return 8

    @staticmethod
    def issigned() -> int:
        return False


class S8(Operand):
    @staticmethod
    def size() -> int:
        return 1

    @staticmethod
    def issigned() -> int:
        return True


class S16(Operand):
    @staticmethod
    def size() -> int:
        return 2

    @staticmethod
    def issigned() -> int:
        return True


class S32(Operand):
    @staticmethod
    def size() -> int:
        return 4

    @staticmethod
    def issigned() -> int:
        return True


class S64(Operand):
    @staticmethod
    def size() -> int:
        return 8

    @staticmethod
    def issigned() -> int:
        return True


class Instruction:
    """Opcode coupled with its operands, if any."""
    def __init__(self, op: Opcode, *operands: Operand) -> None:
        self.op = op
        self.operands = operands

    def __repr__(self) -> str:
        return f"{type(self).__name__}({', '.join(map(repr, (self.op, *self.operands)))})"

    def __str__(self) -> str:
        return f"({' '.join((self.op.name, *map(str, self.operands)))})"


class Block:
    """Block of Bude Word-oriented IR code.

    Class Variables:

    INSTRUCTION_TABLE:
      A table associating each opcode with a tuple of methods for reading
    any operands. The reader methods should be called in the order they
    appear in the tuple. Each of these reader methods take an instruction
    pointer and return the updated instruction pointer along with the
    value of the operand.
    """

    def __init__(self, code: bytes) -> None:
        self.code = code

    def __iter__(self) -> Iterator[Instruction]:
        ip = 0
        while ip < len(self.code):
            ip, instruction = self.decode(ip)
            yield instruction

    def decode(self, ip: int) -> tuple[int, Instruction]:
        """Decode an instruction from self starting at ip.

        The return value is a tuple containg the value of ip needed to
        read the next instruction, followed by the decoded instruction,
        which comprises an opcode and a tuple of operands.
        """
        op = Opcode(self.code[ip])
        operand_types = self.INSTRUCTION_TABLE[op]
        operands = []
        ip += 1
        for operand_type in operand_types:
            size = operand_type.size()
            operands.append(operand_type.from_bytes(self.code[ip:ip+size], byteorder="little"))
            ip += size
        return ip, Instruction(op, *operands)

    @staticmethod
    def encode(ins: Instruction) -> bytes:
        result = ins.op.to_bytes()
        for operand in ins.operands:
            result += operand.to_bytes(byteorder="little")
        return result

    @property
    def size(self) -> int:
        return len(self.code)

    INSTRUCTION_TABLE: dict[Opcode, tuple[type(Operand), ...]] = {
        Opcode.NOP:                (),
        Opcode.PUSH8:              (U8,),
        Opcode.PUSH16:             (U16,),
        Opcode.PUSH32:             (U32,),
        Opcode.PUSH64:             (U64,),
        Opcode.PUSH_INT8:          (S8,),
        Opcode.PUSH_INT16:         (S16,),
        Opcode.PUSH_INT32:         (S32,),
        Opcode.PUSH_INT64:         (S64,),
        Opcode.PUSH_FLOAT32:       (U32,),
        Opcode.PUSH_FLOAT64:       (U64,),
        Opcode.PUSH_CHAR8:         (U8,),
        Opcode.PUSH_CHAR16:        (U16,),
        Opcode.PUSH_CHAR32:        (U32,),
        Opcode.LOAD_STRING8:       (U8,),
        Opcode.LOAD_STRING16:      (U16,),
        Opcode.LOAD_STRING32:      (U32,),
        Opcode.POP:                (),
        Opcode.POPN8:              (S8,),
        Opcode.POPN16:             (S16,),
        Opcode.POPN32:             (S32,),
        Opcode.ADD:                (),
        Opcode.ADDF32:             (),
        Opcode.ADDF64:             (),
        Opcode.AND:                (),
        Opcode.DEREF:              (),
        Opcode.DIVF32:             (),
        Opcode.DIVF64:             (),
        Opcode.DIVMOD:             (),
        Opcode.IDIVMOD:            (),
        Opcode.EDIVMOD:            (),
        Opcode.DUPE:               (),
        Opcode.DUPEN8:             (S8,),
        Opcode.DUPEN16:            (S16,),
        Opcode.DUPEN32:            (S32,),
        Opcode.EQUALS:             (),
        Opcode.EXIT:               (),
        Opcode.FOR_DEC_START:      (S16,),
        Opcode.FOR_DEC:            (S16,),
        Opcode.FOR_INC_START:      (S16,),
        Opcode.FOR_INC:            (S16,),
        Opcode.GET_LOOP_VAR:       (U16,),
        Opcode.GREATER_EQUALS:     (),
        Opcode.GREATER_THAN:       (),
        Opcode.HIGHER_SAME:        (),
        Opcode.HIGHER_THAN:        (),
        Opcode.JUMP:               (S16,),
        Opcode.JUMP_COND:          (S16,),
        Opcode.JUMP_NCOND:         (S16,),
        Opcode.LESS_EQUALS:        (),
        Opcode.LESS_THAN:          (),
        Opcode.LOCAL_GET:          (U16,),
        Opcode.LOCAL_SET:          (U16,),
        Opcode.LOWER_SAME:         (),
        Opcode.LOWER_THAN:         (),
        Opcode.MULT:               (),
        Opcode.MULTF32:            (),
        Opcode.MULTF64:            (),
        Opcode.NOT:                (),
        Opcode.NOT_EQUALS:         (),
        Opcode.OR:                 (),
        Opcode.PRINT:              (),
        Opcode.PRINT_CHAR:         (),
        Opcode.PRINT_FLOAT:        (),
        Opcode.PRINT_INT:          (),
        Opcode.PRINT_STRING:       (),
        Opcode.SUB:                (),
        Opcode.SUBF32:             (),
        Opcode.SUBF64:             (),
        Opcode.SWAP:               (),
        Opcode.SWAP_COMPS8:        (S8,  S8),
        Opcode.SWAP_COMPS16:       (S16, S16),
        Opcode.SWAP_COMPS32:       (S32, S32),
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
        Opcode.FPROM:              (),
        Opcode.FPROML:             (),
        Opcode.FDEM:               (),
        Opcode.ICONVF32:           (),
        Opcode.ICONVF32L:          (),
        Opcode.ICONVF64:           (),
        Opcode.ICONVF64L:          (),
        Opcode.FCONVI32:           (),
        Opcode.FCONVI64:           (),
        Opcode.ICONVC32:           (),
        Opcode.CHAR_8CONV32:       (),
        Opcode.CHAR_32CONV8:       (),
        Opcode.CHAR_16CONV32:      (),
        Opcode.CHAR_32CONV16:      (),
        Opcode.PACK1:              (U8,) * 1,
        Opcode.PACK2:              (U8,) * 2,
        Opcode.PACK3:              (U8,) * 3,
        Opcode.PACK4:              (U8,) * 4,
        Opcode.PACK5:              (U8,) * 5,
        Opcode.PACK6:              (U8,) * 6,
        Opcode.PACK7:              (U8,) * 7,
        Opcode.PACK8:              (U8,) * 8,
        Opcode.UNPACK1:            (U8,) * 1,
        Opcode.UNPACK2:            (U8,) * 2,
        Opcode.UNPACK3:            (U8,) * 3,
        Opcode.UNPACK4:            (U8,) * 4,
        Opcode.UNPACK5:            (U8,) * 5,
        Opcode.UNPACK6:            (U8,) * 6,
        Opcode.UNPACK7:            (U8,) * 7,
        Opcode.UNPACK8:            (U8,) * 8,
        Opcode.PACK_FIELD_GET:     (U8, U8),
        Opcode.COMP_FIELD_GET8:    (U8,),
        Opcode.COMP_FIELD_GET16:   (U16,),
        Opcode.COMP_FIELD_GET32:   (U32,),
        Opcode.PACK_FIELD_SET:     (U8, U8),
        Opcode.COMP_FIELD_SET8:    (U8,),
        Opcode.COMP_FIELD_SET16:   (U16,),
        Opcode.COMP_FIELD_SET32:   (U32,),
        Opcode.COMP_SUBCOMP_GET8:  (U8,  U8),
        Opcode.COMP_SUBCOMP_GET16: (U16, U16),
        Opcode.COMP_SUBCOMP_GET32: (U32, U32),
        Opcode.COMP_SUBCOMP_SET8:  (U8,  U8),
        Opcode.COMP_SUBCOMP_SET16: (U16, U16),
        Opcode.COMP_SUBCOMP_SET32: (U32, U32),
        Opcode.CALL8:              (U8,),
        Opcode.CALL16:             (U16,),
        Opcode.CALL32:             (U32,),
        Opcode.EXTCALL8:           (U8,),
        Opcode.EXTCALL16:          (U16,),
        Opcode.EXTCALL32:          (U32,),
        Opcode.RET:                (),
    }


class TypeKind(enum.IntEnum):
    UNINIT = -1
    SIMPLE = enum.auto()
    PACK   = enum.auto()
    COMP   = enum.auto()

@dataclasses.dataclass
class UserDefinedType:
    """A Bude 'UserDefinedType' which can be a pack or comp."""

    kind: TypeKind
    word_count: int
    fields: list[int]

    def __iter__(self):
        return iter(self.fields)


@dataclasses.dataclass
class Function:
    """A Bude 'Function' object which contains bytecode for a function and metadata."""

    code: Block
    max_for_loop_level: int
    locals_size: int
    locals: list[int]

    def __iter__(self):
        return iter(self.code)


class FunctionBuilder:
    def __init__(self) -> None:
        self.code = bytearray()
        self.max_for_loop_level = 0
        self.locals_size = 0
        self.locals = []

    @classmethod
    def from_function(cls, function: Function) -> Self:
        builder = cls()
        builder.code[:] = bytearray(function.code.code)
        builder.max_for_loop_level = function.max_for_loop_level
        builder.locals_size = function.locals_size
        builder.locals[:] = function.locals
        return builder

    def add_instruction(self, ins: Instruction) -> None:
        self.code.extend(Block.encode(ins))

    def build(self) -> Function:
        return Function(Block(bytes(self.code)),
                        self.max_for_loop_level,
                        self.locals_size,
                        self.locals[:])


@dataclasses.dataclass
class Signature:
    """The signature of a Bude (external) function."""

    params: list[int]
    rets: list[int]

    def __str__(self) -> str:
        params_str = " ".join(str(t) for t in self.params)
        rets_str = " ".join(str(t) for t in self.rets)
        return f"{{params ({params_str})  rets ({rets_str})}}"


class CallingConvention(enum.IntEnum):
    BUDE = 0
    NATIVE = enum.auto()
    MS_X64 = enum.auto()
    SYSV_AMD64 = enum.auto()


@dataclasses.dataclass
class ExternalFunction:
    """A Bude 'ExternalFunction' object which refers to a non-Bude function defined elsewhere."""

    sig: Signature
    name: str
    call_conv: CallingConvention


@dataclasses.dataclass
class ExternalLibrary:
    """A library containing external functions."""

    indices: list[int]
    filename: str


@dataclasses.dataclass
class Module:
    """A Bude 'Module' object which contains a list of strings and functions."""

    strings: list[str]
    functions: list[Function]
    user_defined_types: list[UserDefinedType]
    externals: list[ExternalFunction]
    ext_libraries: list[ExternalLibrary]

    def pprint(self, file=sys.stdout) -> None:
        print("STRINGS", file=file)
        for i, string in enumerate(self.strings):
            print(f"{i: 4}: {string!r}", file=file)
        print("FUNCTIONS", file=file)
        for i, function in enumerate(self.functions):
            print(f"{i: 4}: {{max-for-loop-level {function.max_for_loop_level}",
                  f"locals-size {function.locals_size}",
                  f"locals ({' '.join(str(local) for local in function.locals)})}}",
                  file=file, sep="  ")
            for ins in function:
                print(f"      {ins}", file=file)
        print("USER-DEFINED-TYPES", file=file)
        for i, ud_type in enumerate(self.user_defined_types, start=BUILTIN_TYPE_COUNT):
            print(f"{i: 4}: {{kind {ud_type.kind.name}",
                  f"word-count {ud_type.word_count}",
                  f"fields ({' '.join(str(field) for field in ud_type)})}}",
                  file=file, sep="  ")
        print("EXTERNAL-FUNCTIONS")
        for i, external in enumerate(self.externals):
            print(f"{i: 4}: {{sig {external.sig!s}  name {external.name!r}",
                  f"call-conv {external.call_conv.name}}}", file=file, sep="  ")
        print("EXTERNAL-LIBRARIES")
        for i, library in enumerate(self.ext_libraries):
            print(f"{i: 4}: {{indices ({' '.join(str(j) for j in library.indices)})",
                  f"filename {library.filename!r}}}", file=file, sep="  ")


class ModuleBuilder:
    def __init__(self) -> None:
        self.strings = []
        self.functions = [FunctionBuilder()]
        self.user_defined_types = []
        self.externals = []
        self.ext_libraries = []

    def from_module(cls, module: Module) -> Self:
        builder = cls()
        builder.strings[:] = module.strings
        builder.functions[:] = (FunctionBuilder.from_function(function)
                                for function in module.functions)
        builder.user_defined_types[:] = module.user_defined_types
        builder.externals[:] = module.externals
        builder.ext_libraries[:] = module.ext_libraries
        return builder

    def add_string(self, string: str) -> int:
        self.strings.append(string)
        return len(self.strings) - 1

    def add_instruction(self, ins: Instruction) -> None:
        self.functions[-1].add_instruction(ins)

    def new_function(self) -> int:
        self.functions.append(FunctionBuilder())
        return len(self.functions) - 1

    def add_type(self, ud_type: UserDefinedType) -> int:
        self.user_defined_types.append(ud_type)
        return len(self.user_defined_types) - 1

    def add_external(self, external: ExternalFunction) -> int:
        index = len(self.externals)
        self.externals.append(external)
        self.ext_libraries[-1].indices.append(index)
        return index

    def new_ext_library(self, filename: str) -> int:
        self.ext_libraries.append(ExternalLibrary([], filename))
        return len(self.ext_libraries) - 1

    def build(self) -> Module:
        return Module(self.strings[:],
                      [function.build() for function in self.functions],
                      self.user_defined_types[:],
                      self.externals[:],
                      self.ext_libraries[:])
