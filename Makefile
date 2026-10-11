.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c17 -O3 -Wall -Wextra
LDFLAGS ?=
DEPFLAGS ?= -MMD -MP
TEST_ARGS ?= -v

# Select commands for the host shell, independently of the compiler.
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /d /c
EXEEXT ?= .exe
LDLIBS ?=
OPEN ?= start ""

shellpath = $(subst /,\,$(1))
mkdir_p = if not exist "$(call shellpath,$(1))" mkdir "$(call shellpath,$(1))"
rm_file = if exist "$(call shellpath,$(1))" del /q "$(call shellpath,$(1))"
rm_tree = if exist "$(call shellpath,$(1))" rmdir /s /q "$(call shellpath,$(1))"
else
EXEEXT ?=
LDLIBS ?= -lm
OPEN ?= $(if $(filter Darwin,$(shell uname -s)),open,xdg-open)

shellpath = $(1)
mkdir_p = mkdir -p "$(1)"
rm_file = $(RM) "$(1)"
rm_tree = $(RM) -r "$(1)"
endif

SRC := src
OBJ := obj
BIN := bin

PROGRAMS := main benchmark rng_image
BINS := $(addprefix $(BIN)/,$(addsuffix $(EXEEXT),$(PROGRAMS)))

RNG_SRCS := $(SRC)/rng/rng.c $(SRC)/rng/rng_zig_layers.c
RNG_OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(RNG_SRCS))

GEN_SRC := $(SRC)/rng/rng_zig_layers.c
GENERATOR := $(BIN)/rng_zig_layers_gen$(EXEEXT)

TEST_BIN := $(BIN)/test_rng$(EXEEXT)
TEST_OBJS := $(OBJ)/tests/test_rng.o

ALL_OBJS := $(addprefix $(OBJ)/,$(addsuffix .o,$(PROGRAMS))) \
            $(RNG_OBJS) $(OBJ)/plt/plt.o $(OBJ)/rng_zig_layers_gen.o \
			$(TEST_OBJS)
DEPS := $(ALL_OBJS:.o=.d)

OBJ_DIRS := $(sort $(dir $(ALL_OBJS)))
BUILD_DIRS := $(sort $(OBJ_DIRS) $(BIN)/)

all: $(BINS)

test: $(TEST_BIN)
	@"$(call shellpath,$<)" $(TEST_ARGS)

run: $(BIN)/main$(EXEEXT)
	@"$(call shellpath,$<)"

benchmark: $(BIN)/benchmark$(EXEEXT)
	@"$(call shellpath,$<)"

image: image.ppm

image.ppm: $(BIN)/rng_image$(EXEEXT)
	@"$(call shellpath,$<)" "$@"

# Override OPEN when needed: make open OPEN=xdg-open
open: image.ppm
	$(OPEN) "$(call shellpath,$<)"

hot:
ifeq ($(OS),Windows_NT)
	@echo make hot requires find and entr in a POSIX shell.
	@exit /b 1
else
	@find "$(SRC)" -type f | entr -c $(MAKE) run
endif

$(BIN)/main$(EXEEXT): $(OBJ)/main.o $(OBJ)/plt/plt.o $(RNG_OBJS)
$(BIN)/benchmark$(EXEEXT): $(OBJ)/benchmark.o $(OBJ)/plt/plt.o $(RNG_OBJS)
$(BIN)/rng_image$(EXEEXT): $(OBJ)/rng_image.o $(RNG_OBJS)
$(GENERATOR): $(OBJ)/rng_zig_layers_gen.o
$(TEST_BIN): $(TEST_OBJS) $(RNG_OBJS)

# Order-only prerequisites create directories once during parallel builds.
# Directory timestamp changes do not trigger recompilation or relinking.
$(filter-out $(OBJ)/,$(OBJ_DIRS)): | $(OBJ)/
$(ALL_OBJS): | $(OBJ_DIRS)
$(BINS) $(GENERATOR) $(TEST_BIN): | $(BIN)/

$(BUILD_DIRS):
	@$(call mkdir_p,$@)

$(BINS) $(GENERATOR) $(TEST_BIN):
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o "$@"

$(OBJ)/%.o: $(SRC)/%.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c "$<" -o "$@"

$(OBJ)/tests/%.o: tests/%.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -I"$(SRC)" -c "$<" -o "$@"

# Normal builds use the committed table. Generate it only if missing.
ifeq ($(wildcard $(GEN_SRC)),)
$(GEN_SRC): regen
endif

# Replace the table only after the generator succeeds.
regen: $(GENERATOR)
ifeq ($(OS),Windows_NT)
	@"$(call shellpath,$(GENERATOR))" > "$(call shellpath,$(GEN_SRC)).tmp" && (move /y "$(call shellpath,$(GEN_SRC)).tmp" "$(call shellpath,$(GEN_SRC))" >nul) || (if exist "$(call shellpath,$(GEN_SRC)).tmp" del /q "$(call shellpath,$(GEN_SRC)).tmp" & exit /b 1)
else
	@set -e; tmp="$(GEN_SRC).tmp"; \
	trap 'rm -f "$$tmp"' 0 1 2 3 15; \
	"$(GENERATOR)" > "$$tmp"; \
	mv "$$tmp" "$(GEN_SRC)"
endif

clean:
	$(call rm_tree,$(BIN))
	$(call rm_tree,$(OBJ))
	$(call rm_file,image.ppm)
	$(call rm_file,$(GEN_SRC).tmp)

clean-gen: clean
	$(call rm_file,$(GEN_SRC))

-include $(DEPS)

.PHONY: all test run benchmark image open hot regen clean clean-gen