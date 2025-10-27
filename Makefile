# ===== CONFIG =====
CC      := gcc
CFLAGS  := -std=c99 -O2 -Wall -Wextra -Iincludes
LDFLAGS :=
TARGET  := emulator

SRCS    := main.c $(wildcard core/*.c)
OBJDIR  := build
OBJS    := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

# ===== DEFAULT =====
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# compile .c -> build/.o and generate dep files alongside
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# include auto-generated dependencies
-include $(DEPS)

# ===== CLEAN =====
clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean

