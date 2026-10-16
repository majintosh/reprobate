CC = gcc
CFLAGS = -Iinclude -g

NAME = reprobate

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)

$(NAME): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

test_patcher:
	make
	$(CC) -g -o testo test.c; ./testo & sudo gdb -ex "break main" -ex "layout src" --args ./$(NAME) $$! ./testo bad

test_patchee:
	make
	$(CC) -g -o testo test.c; ./testo & TESTOPID=$$! && export TESTOPID && echo $$TESTOPID && sudo ./$(NAME) $$TESTOPID ./testo bad && sudo gdb -p $$TESTOPID -ex "layout src"

clean:
	rm -rf $(NAME)

.PHONY: test clean
