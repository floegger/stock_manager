CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic

SRC_DIR   := src
INC_DIR   := include
OBJ_DIR   := obj
BUILD_DIR := build

SRCS     := $(wildcard $(SRC_DIR)/*.cpp) main.cpp
OBJS     := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
TARGET   := $(BUILD_DIR)/stock_manager

TEST_DIR    := tests
TEST_SRCS   := $(wildcard $(TEST_DIR)/*.cpp)
# Tests share all src/*.cpp but NOT main.cpp
LIB_OBJS    := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.cpp))
TEST_OBJS   := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_DEPS   := $(TEST_OBJS:.o=.d)
TEST_TARGET := $(BUILD_DIR)/run_tests

# ── Build modes ──────────────────────────────────────────────────
# Default: debug
MODE     ?= debug

ifeq ($(MODE),release)
    CXXFLAGS += -O2 -DNDEBUG
else
    CXXFLAGS += -O0 -g
endif

# ── Optional AVX2 ────────────────────────────────────────────────
# Enable with:  make AVX2=1
ifeq ($(AVX2),1)
    CXXFLAGS += -mavx2 -D__AVX2__
endif

# ── Dependency flags ─────────────────────────────────────────────
DEPFLAGS  = -MMD -MP

# ── Default target ───────────────────────────────────────────────
.PHONY: all
all: $(TARGET) $(TEST_TARGET)

# ── Link ─────────────────────────────────────────────────────────
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# ── Compile src/*.cpp ────────────────────────────────────────────
$(OBJ_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I$(INC_DIR) -c $< -o $@

# ── Compile main.cpp ─────────────────────────────────────────────
$(OBJ_DIR)/main.o: main.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I$(INC_DIR) -c $< -o $@

# ── Compile tests/%.cpp ──────────────────────────────────────────
$(OBJ_DIR)/$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I$(INC_DIR) -c $< -o $@

# ── Link test binary ─────────────────────────────────────────────
$(TEST_TARGET): $(TEST_OBJS) $(LIB_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# ── Create build dir ─────────────────────────────────────────────
$(BUILD_DIR):
	@mkdir -p $@

# ── Include auto-generated dependencies ──────────────────────────
-include $(DEPS)
-include $(TEST_DEPS)

# ── Convenience targets ──────────────────────────────────────────
.PHONY: test
test: $(TEST_TARGET)
	./$(TEST_TARGET)

.PHONY: release
release:
	$(MAKE) MODE=release

.PHONY: avx2
avx2:
	$(MAKE) AVX2=1

.PHONY: avx2-release
avx2-release:
	$(MAKE) MODE=release AVX2=1

.PHONY: clean
clean:
	$(RM) -r $(OBJ_DIR) $(BUILD_DIR)

.PHONY: help
help:
	@echo "Targets:"
	@echo "  all            Debug build + test binary (default)"
	@echo "  test           Build and run unit tests"
	@echo "  release        Optimised build  (MODE=release)"
	@echo "  avx2           Debug build with AVX2 SIMD"
	@echo "  avx2-release   Release build with AVX2 SIMD"
	@echo "  clean          Remove build artefacts"
	@echo ""
	@echo "Variables (can be passed on the command line):"
	@echo "  MODE=debug|release   (default: debug)"
	@echo "  AVX2=1               Enable AVX2 SIMD path"
