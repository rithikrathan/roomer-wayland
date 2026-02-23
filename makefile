EXEC       = wayland-boomer
GIT_COMMIT = $(shell git rev-parse HEAD)

.DEFAULT_GOAL := default
.PHONY: default
default:
	@echo "Available targets:"
	@echo "  - make run"
	@echo "  - make build"
	@echo "  - make install"

.PHONY: run
run: $(EXEC)
	grim - | ./$(EXEC)

.PHONY: build
build: $(EXEC)

.PHONY: install
install: $(EXEC)
	sudo cp $(EXEC) /usr/bin/

$(EXEC):
	clang ./src/main.c ./src/globals.c ./src/args.c ./src/controls.c ./src/image.c ./src/draw.c \
		-o $(EXEC) \
		-std=c23 -pedantic -Wall -Wextra -Wpedantic -ggdb -O3 \
		-flto -fPIE -lm -lglfw -I./vendor/ -L./vendor/ -lraylib \
		-fcf-protection=full -fstack-protector-strong -fno-omit-frame-pointer \
		-DVERSION="\"$(GIT_COMMIT)\""
