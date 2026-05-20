# Compiler and flags
CXX := g++
CC := gcc

# Installation prefix
PREFIX ?= /usr/bin

# Build directories and external libs dir
BUILD_DIR := build
LIB_DIR := lib
WEB_DIR := src

# Web resources
INDEX_HTML      := $(WEB_DIR)/index.html
MIN_HTML        := $(BUILD_DIR)/index.html.min
GZ_HTML         := $(BUILD_DIR)/index.html.gz
INDEX_HTML_HDR  := $(BUILD_DIR)/index_html_gz.h

BUILD ?= release
UNAME_S := $(shell uname -s)

ifeq ($(BUILD),debug)
  CXXFLAGS := -I./include -I./$(LIB_DIR) -I./$(BUILD_DIR) -std=c++20 -Wall -Wextra -MD -g -O0
  CFLAGS   := -I./include -I./$(LIB_DIR) -Wall -Wextra -g -O0
else ifeq ($(BUILD),release)
  CXXFLAGS := -I./include -I./$(LIB_DIR) -I./$(BUILD_DIR) -std=c++20 -Wall -Wextra -Wsign-compare -MD -O3 -flto -fuse-linker-plugin
  CFLAGS   := -I./include -I./$(LIB_DIR) -Wall -Wextra -O3 -flto -fuse-linker-plugin
endif

# Linker flags
ifeq ($(OS),Windows_NT)

# -------- Windows (mingw64) --------
CXXFLAGS += -DCURL_STATICLIB
ifeq ($(BUILD),debug)
	LDFLAGS := $(shell pkg-config --static --libs libcurl zlib) \
          -static -static-libgcc -static-libstdc++ -lws2_32
else
	LDFLAGS := $(shell pkg-config --static --libs libcurl zlib) \
          -static -static-libgcc -static-libstdc++ -lws2_32 -s
endif

else

# -------- Linux / others --------
ifeq ($(BUILD),debug)
	LDFLAGS := $(shell pkg-config --libs libcurl zlib)
else
	LDFLAGS := $(shell pkg-config --libs libcurl zlib) -s
endif

endif

# Executable name
EXEC := 3macloc

# Sources and objects
SRCS := $(wildcard src/*.cpp) lib/picoproto.cc lib/base64.c lib/tinyxml2.cpp
OBJS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(wildcard src/*.cpp)) \
        $(BUILD_DIR)/picoproto.o \
        $(BUILD_DIR)/base64.o \
        $(BUILD_DIR)/tinyxml2.o

# Phony targets
.PHONY: all clean distclean install uninstall debug release web windows-compat

# Default target
all: web $(EXEC)

# --------------------------------------------------
# WINDOWS COMPATIBILITY
# --------------------------------------------------

ifeq ($(OS),Windows_NT)
LDFLAGS += -lwsock32 -lws2_32
endif

windows-compat:
ifeq ($(OS),Windows_NT)
	@sed -i 's/GetMessage/GetMsgProto/g' src/apple.cpp
	@sed -i 's/GetMessage/GetMsgProto/g' lib/picoproto.cc lib/picoproto.h 2>/dev/null || true
endif

# --------------------------------------------------
# WEB PIPELINE
# --------------------------------------------------

# Step 1 — minify (если есть minhtml)
$(MIN_HTML): $(INDEX_HTML) | $(BUILD_DIR)
	@if command -v minhtml >/dev/null 2>&1; then \
		echo "[web] minifying index.html"; \
		minhtml --minify-css --minify-js $< -o $@; \
	else \
		echo "[web] WARNING: minhtml not found, using original HTML"; \
		cp $< $@; \
	fi

# Step 2 — gzip
$(GZ_HTML): $(MIN_HTML)
	@echo "[web] gzip compress"
	gzip -9 -c $< > $@

# Step 3 — convert to C++ header
$(INDEX_HTML_HDR): $(GZ_HTML)
	@echo "[web] generating C++ header"
	xxd -i -n index_html_gz $< \
	| sed 's/^unsigned/const unsigned/' \
	> $@

# Aggregate web target
web: $(INDEX_HTML_HDR)

# --------------------------------------------------
# BUILD EXECUTABLE
# --------------------------------------------------

# Build executable
$(EXEC): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile C++ sources
$(BUILD_DIR)/3macloc.o: $(INDEX_HTML_HDR)
$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR) windows-compat
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Compile C sources
$(BUILD_DIR)/%.o: lib/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: lib/%.cc | $(BUILD_DIR) windows-compat
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: lib/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean and distclean
clean:
	rm -rf $(BUILD_DIR) $(EXEC)

# Install and uninstall
install: $(EXEC)
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(EXEC) $(DESTDIR)$(PREFIX)/bin/$(EXEC)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXEC)

# Mode shortcuts
debug:
	$(MAKE) BUILD=debug all

release:
	$(MAKE) BUILD=release all

# Include dependency files
-include $(OBJS:.o=.d)
