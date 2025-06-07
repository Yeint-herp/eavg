TYPE ?= static
CONFIG ?= debug

CC := cc
LD := cc

CFLAGS := -std=gnu11 -Wall -Wextra -Werror -pthread
LDFLAGS := -lpthread

ifeq ($(TYPE),static)
    $(info Using static build)
    LIBEXT := a
    BUILD_STATIC := 1
else ifeq ($(TYPE),dynamic)
    $(info Using dynamic build)
    LIBEXT := so
    BUILD_DYNAMIC := 1
else
    $(error Unknown TYPE '$(TYPE)'. Expected 'static' or 'dynamic')
endif

ifeq ($(CONFIG),debug)
    $(info Using debug config)
    CFLAGS += -O0 -g
    BUILD_DIR := build/debug/$(TYPE)
else ifeq ($(CONFIG),release)
    $(info Using release config)
    CFLAGS += -O2 -DNDEBUG -s
    BUILD_DIR := build/release/$(TYPE)
else
    $(error Unknown CONFIG '$(CONFIG)'. Expected 'debug' or 'release')
endif

LIB := $(BUILD_DIR)/libeavg.$(LIBEXT)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(wildcard *.c))

all: $(LIB)

$(LIB): $(OBJS)
ifeq ($(BUILD_STATIC),1)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^
else ifeq ($(BUILD_DYNAMIC),1)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
endif

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

clean:
	rm -rf build

