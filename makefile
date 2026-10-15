CC = gcc
CFLAGS = -Iinclude

NAME = reprobate

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)

$(NAME): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

test:
	make
	$(CC) -o test test.c; gdb $(NAME)

clean:
	rm -rf $(NAME)

.PHONY: clean
