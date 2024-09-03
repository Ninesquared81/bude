.PHONY: all

all: game.exe ;

game.exe: game.asm
	fasm game.asm

game.asm: game.bude ../bin/bude.exe
	../bin/bude.exe game.bude -a -o game.asm --lib raylib=raylib.dll
