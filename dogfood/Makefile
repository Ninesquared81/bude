.PHONY: all

all: raylib-game.exe ;

raylib-game.exe: raylib-game.asm
	fasm raylib-game.asm

raylib-game.asm: raylib-game.bude ../bin/bude.exe
	../bin/bude.exe raylib-game.bude -a -o raylib-game.asm --lib raylib=raylib.dll
