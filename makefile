CC := clang
UNAME_S :=
ifneq ($(OS),Windows_NT)
    UNAME_S := $(shell uname -s)
endif

DEFINES = 
CFLAGS = #-fsanitize=address
LDFLAGS =
ifeq ($(OS),Windows_NT)
    PLATFORM := win32
	EXT := .exe
	# windows is weird
    HAS_SH := $(shell where sh.exe 2>nul)
    ifeq ($(HAS_SH),)
        $(info Using CMD commands.)
        rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
        SRC := $(call rwildcard,src,*.c) # $(shell where.exe /r src *.c)
        LDFLAGS += -luser32 -lgdi32 -lkernel32
        MK = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
        RM = if exist "$(subst /,\,$1)" rd /s /q "$(subst /,\,$1)"
    else
        SRC := $(shell find src -type f -name "*c")
        MK = mkdir -p $1
        RM = rm -rf $1
    endif
else
    ifeq ($(UNAME_S),Linux)
        PLATFORM := linux
        SRC := $(shell find src -type f -name "*.c")
		LDFLAGS += -lxcb -lxcb-randr -lm
        MK = mkdir -p $(1)
		RM = rm -rf $1
    else ifeq ($(UNAME_S),Darwin)
        PLATFORM := macos
        SRC := $(shell find src -type f \( -name "*.c" -o -name "*.m" \))
        LDFLAGS += -framework Cocoa -framework CoreGraphics -framework QuartzCore
        MK = mkdir -p $(1)
		RM = rm -rf $1
    else
        $(error Unsupported platform: $(UNAME_S))
    endif
endif

INCLUDE := -Isrc
BIN := bin
BUILD := build

# <c/m>.o to prevent possible overlap with files that have the same name but different extension
OBJ := $(patsubst src/%.c,$(BUILD)/%.c.o,$(filter %.c,$(SRC)))
OBJ += $(patsubst src/%.m,$(BUILD)/%.m.o,$(filter %.m,$(SRC)))

TARGET := $(BIN)/prog$(EXT)


release: $(TARGET)

debug: CFLAGS += -g
debug: $(TARGET)


$(TARGET): $(OBJ)
	@$(call MK,$(BIN))
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD)/%.c.o: src/%.c
	@$(call MK,$(dir $@))
	$(CC) -c $< -o $@ $(DEFINES) $(CFLAGS) $(INCLUDE)

$(BUILD)/%.m.o: src/%.m
	@$(call MK,$(dir $@))
	$(CC) -c $< -o $@ $(DEFINES) $(CFLAGS) $(INCLUDE)

drun: $(TARGET)
	lldb $(TARGET) ../examples/Cairo.jpg

clean:
	@$(call RM,$(BIN))
	@$(call RM,$(BUILD))

.PHONY: clean