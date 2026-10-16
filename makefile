CC = gcc
CFLAGS = -Iinclude -g

NAME = reprobate

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)

$(NAME): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

testo: test.c
	$(CC) -g -o $@ $<

test_patcher: $(NAME) testo
	./testo & sudo gdb -ex "break main" -ex "layout src" --args ./$(NAME) $$! ./testo bad; \
	kill $$(pidof testo)

test_patchee: $(NAME) testo
	./testo & TESTOPID=$$!; \
	echo $$TESTOPID; \
	sudo ./$(NAME) $$TESTOPID ./testo bad && \
	sudo gdb -p $$TESTOPID -ex "layout asm" -ex "disassemble main"; \
	kill $$(pidof testo)

clean:
	rm -rf $(NAME) testo

.PHONY: test clean
