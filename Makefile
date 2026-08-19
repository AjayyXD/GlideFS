CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
INCLUDES = -Iinclude
LDFLAGS = -lpthread
LDLIBS = -lpthread

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

BUILD_DIR = build
BIN_DIR = bin

TARGET = $(BIN_DIR)/glidefsctl
SRCS = src/main.c src/cli.c src/util.c src/state.c src/hotspot.c src/share.c src/dashboard.c src/client.c src/deps.c src/network.c
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

HOTSPOTCTL_DIR = third_party/hotspotctl
HOTSPOTCTL_BIN = $(HOTSPOTCTL_DIR)/hotspotctl

.PHONY: all clean install uninstall hotspotctl

all: $(TARGET) hotspotctl

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

hotspotctl: $(HOTSPOTCTL_BIN) | $(BIN_DIR)
	cp -f $(HOTSPOTCTL_BIN) $(BIN_DIR)/hotspotctl

$(HOTSPOTCTL_BIN):
	$(MAKE) -C $(HOTSPOTCTL_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	$(MAKE) -C $(HOTSPOTCTL_DIR) clean

install: all
	install -Dm755 $(BIN_DIR)/glidefsctl $(DESTDIR)$(BINDIR)/glidefsctl
	install -Dm755 $(BIN_DIR)/hotspotctl $(DESTDIR)$(BINDIR)/hotspotctl

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/glidefsctl $(DESTDIR)$(BINDIR)/hotspotctl
