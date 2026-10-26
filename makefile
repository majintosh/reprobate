CC = gcc
CFLAGS = -Iinclude -g

NAME = reprobate
FUNCTION = bado

SRC = $(wildcard src/*.c)
HDR = $(wildcard include/*.h)

$(NAME): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

testo: test.c
	$(CC) -g -o $@ $<

test_patcher: $(NAME) testo
	./testo & TESTOPID=$$!; \
	sudo gdb -ex "break main" -ex "layout src" -ex "run" --args ./$(NAME) $$TESTOPID ./testo $(FUNCTION); \
	kill $$(pidof testo)

test_patchee: $(NAME) testo
	./testo & TESTOPID=$$!; \
	echo $$TESTOPID; \
	sudo ./$(NAME) $$TESTOPID ./testo $(FUNCTION) && \
	sudo gdb -p $$TESTOPID -ex "layout asm" -ex "disassemble main" -ex "break $(FUNCTION)"; \
	kill $$(pidof testo)

test: $(NAME) testo
	./testo & TESTOPID=$$!; \
	sudo gdb -ex "break main" -ex "layout src" --args ./$(NAME) $$TESTOPID ./testo $(FUNCTION); \
	sudo gdb -p $$TESTOPID -ex "layout asm" -ex "disassemble main"; \
	kill $$(pidof testo)

clean:
	rm -rf $(NAME) testo

.PHONY: test test_patcher test_patchee clean
