CC = gcc -O3
BUILD_DIR = build

all: build

build_main:
	@mkdir -p $(BUILD_DIR)
	@$(CC) -o ./$(BUILD_DIR)/main.out main.c

run: build_main
	@./$(BUILD_DIR)/main.out

.PHONY: clean
clean:
	@rm -rf ./$(BUILD_DIR)/*.out