#
#  GLAZER
#
#  This makefile requires GNU make.
#

PROGRAM=glazer

# CC=gcc
# CC=clang
# CC=x86_64-w64-mingw32-gcc
# CC=musl-gcc -static

BASEFLAG=-std=c99
OPTIMISE=-O1 -g0
WARNINGS=-Wall -Wextra

CFLAGS   ?= $(BASEFLAG) $(OPTIMISE) $(WARNINGS)
CPPFLAGS ?=
LDFLAGS  ?= $(OPTIMISE)
LIBS     ?= -lm


BUILD_DIR=_build

DUMMY=$(BUILD_DIR)/__dummy

all: $(DUMMY) $(PROGRAM)


#----- Objects ---------------------------------------------------------

MAIN_OBJS = \
	$(BUILD_DIR)/assemble.o   \
	$(BUILD_DIR)/chars.o      \
	$(BUILD_DIR)/defs.o       \
	$(BUILD_DIR)/eval.o       \
	$(BUILD_DIR)/errors.o     \
	$(BUILD_DIR)/file.o       \
	$(BUILD_DIR)/huff.o       \
	$(BUILD_DIR)/intern.o     \
	$(BUILD_DIR)/lex.o        \
	$(BUILD_DIR)/main.o       \
	$(BUILD_DIR)/node.o       \
	$(BUILD_DIR)/opcodes.o    \
	$(BUILD_DIR)/output.o     \
	$(BUILD_DIR)/parse.o      \
	$(BUILD_DIR)/resolve.o    \
	$(BUILD_DIR)/symbol.o     \
	$(BUILD_DIR)/utility.o    \
	$(BUILD_DIR)/z_opcode.o   \
	$(BUILD_DIR)/z_text.o

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<


#----- Targets ---------------------------------------------------------

clean:
	rm -f $(PROGRAM) *.exe *.ulx $(BUILD_DIR)/*.o

$(PROGRAM): $(MAIN_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

# this is used to create the BUILD_DIR directory
$(DUMMY):
	mkdir -p $(BUILD_DIR)
	@touch $@

.PHONY: all clean

#--- editor settings ------------
# vi:ts=8:sw=8:noexpandtab
