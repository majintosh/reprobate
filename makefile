CC = gcc
CFLAGS = -Iinclude

NAME = reprobate

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)

$(NAME): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

clean:
	rm -rf $(NAME)

.PHONY: clean
