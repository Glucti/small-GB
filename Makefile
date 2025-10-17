# ===== BASIC CONFIG =====
CC       := gcc
CFLAGS   := -Wall -Wextra -std=c99 -O2
TARGET   := emulator
TESTBIN  := tests

SRC      := cpu.c
TESTSRC  := $(SRC) cpu_tests.c

# ===== DEFAULT BUILD =====
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

# ===== RUN TESTS =====
test: $(TESTBIN)
	./$(TESTBIN)

$(TESTBIN): $(TESTSRC)
	$(CC) $(CFLAGS) -DTESTING $^ -o $@

# ===== CLEAN =====
clean:
	rm -f $(TARGET) $(TESTBIN)

# ===== PHONY TARGETS =====
.PHONY: all test clean

