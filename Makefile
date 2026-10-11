.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c17 -O3 -Wall -Wextra
LDFLAGS ?=
LDLIBS ?= -lm
DEPFLAGS ?= -MMD -MP

SRC := src
OBJ := obj
BIN := bin

PROGRAMS := main benchmark rng_image
BINS := $(addprefix $(BIN)/,$(PROGRAMS))
RNG_SRCS := $(SRC)/rng/rng.c $(SRC)/rng/rng_zig_layers.c
RNG_OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(RNG_SRCS))
GEN_SRC := $(SRC)/rng/rng_zig_layers.c
GENERATOR := $(BIN)/rng_zig_layers_gen
ALL_OBJS := $(addprefix $(OBJ)/,$(addsuffix .o,$(PROGRAMS))) \
            $(RNG_OBJS) $(OBJ)/plt/plt.o $(OBJ)/rng_zig_layers_gen.o
DEPS := $(ALL_OBJS:.o=.d)

# OPEN can be overridden, for example: make open OPEN=xdg-open.
OPEN ?= $(if $(filter Darwin,$(shell uname -s)),open,xdg-open)

all: $(BINS)

run: $(BIN)/main
	@"$<"

benchmark: $(BIN)/benchmark
	@"$<"

image: image.ppm

image.ppm: $(BIN)/rng_image
	@"$<"

open: image.ppm
	$(OPEN) "$<"

hot:
	@find "$(SRC)" -type f | entr -c $(MAKE) run

$(BIN)/main: $(OBJ)/main.o $(OBJ)/plt/plt.o $(RNG_OBJS)
$(BIN)/benchmark: $(OBJ)/benchmark.o $(OBJ)/plt/plt.o $(RNG_OBJS)
$(BIN)/rng_image: $(OBJ)/rng_image.o $(RNG_OBJS)
$(GENERATOR): $(OBJ)/rng_zig_layers_gen.o

$(BINS) $(GENERATOR):
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o "$@"

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c "$<" -o "$@"

# Normal builds use the committed table. Only generate it if it is missing.
$(GEN_SRC):
	$(MAKE) regen

# Write to a temporary file so a failed generator preserves the existing table.
regen: $(GENERATOR)
	@set -e; tmp="$(GEN_SRC).tmp"; \
	trap 'rm -f "$$tmp"' 0 1 2 3 15; \
	"$(GENERATOR)" > "$$tmp"; \
	mv "$$tmp" "$(GEN_SRC)"

clean:
	$(RM) -r "$(BIN)" "$(OBJ)"
	$(RM) image.ppm "$(GEN_SRC).tmp"

clean-gen: clean
	$(RM) "$(GEN_SRC)"

-include $(DEPS)

.PHONY: all run benchmark image open hot regen clean clean-gen