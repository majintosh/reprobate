CC = gcc
CFLAGS = -Iinclude -g

NAME = reprobate

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)

$(NAME): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

test:
	make
	$(CC) -o testo test.c; ./testo & sudo gdb -ex "break main" -ex "layout src" --args ./$(NAME) $$! ~/testo bad

clean:
	rm -rf $(NAME)

.PHONY: clean
