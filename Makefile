# slop — M3G decoder / glTF converter (C++17)
#
# Build:
#   make            # M3G viewer -> build/debug/slop-view  (sokol)
#   make view       # same as make debug
#   make release    # CLI binary -> build/release/slop
#   make clean
#
# Viewer:
#   ./build/debug/slop-view assets/90.m3g
#
# CMake (LSP compile_commands, optional ctest):
#   cmake -S . -B build && cmake --build build
#
# Vendored headers:
#   make setup

.PHONY: all debug release view clean setup setup-libs setup-cgltf setup-cjson setup-stb setup-sokol test

APP      := slop
VIEW_APP := slop-view
BUILDROOT := build
SRCDIR   := src
INCDIR   := include
TEST_BINDIR := tests/bin

CXX      ?= c++
CC       ?= gcc
BUILD    ?= debug
BUILDDIR := $(BUILDROOT)/$(BUILD)

APP_CXXFLAGS_debug   := -std=c++17 -Wall -Wextra -O0 -g -D_DEFAULT_SOURCE -DAPP_NAME=\"$(APP)\"
APP_CXXFLAGS_release := -std=c++17 -Wall -Wextra -Os -g0 -DNDEBUG -D_DEFAULT_SOURCE -DAPP_NAME=\"$(APP)\"
APP_CFLAGS_debug     := -std=c99 -Wall -Wextra -O0 -g -D_DEFAULT_SOURCE
APP_CFLAGS_release   := -std=c99 -Wall -Wextra -Os -g0 -DNDEBUG -D_DEFAULT_SOURCE
APP_CXXFLAGS         := $(APP_CXXFLAGS_$(BUILD))
APP_CFLAGS           := $(APP_CFLAGS_$(BUILD))
APP_CPPFLAGS         := -I. -I$(INCDIR) -I$(SRCDIR) -Ivendors -Ivendors/libs
APP_LDFLAGS          :=
APP_LIBS             := -lz -lm
VIEW_LIBS            := $(APP_LIBS) -lGL -lX11 -lXi -lXcursor -ldl -lpthread

CXX_SRCS := $(shell find $(SRCDIR) -name '*.cpp' 2>/dev/null)
# Exclude sokol viewer from the CLI app.
C_SRCS   := $(filter-out $(SRCDIR)/debug.c,$(shell find $(SRCDIR) -name '*.c' 2>/dev/null))
VENDOR_C_SRCS := vendors/cjson/cJSON.c
CXX_OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(CXX_SRCS))
C_OBJS   := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(C_SRCS))
VENDOR_C_OBJS := $(patsubst vendors/%.c,$(BUILDDIR)/vendors/%.o,$(VENDOR_C_SRCS))
OBJS := $(CXX_OBJS) $(C_OBJS) $(VENDOR_C_OBJS)
BIN  := $(BUILDDIR)/$(APP)
VIEW_BIN := $(BUILDDIR)/$(VIEW_APP)
VIEW_LIB_OBJS := $(filter-out $(BUILDDIR)/main.o,$(OBJS))
VIEW_DEBUG_OBJ := $(BUILDDIR)/debug_view.o

all: debug

debug:
	@$(MAKE) --no-print-directory BUILD=debug $(VIEW_BIN)

release:
	@$(MAKE) --no-print-directory BUILD=release $(BUILDROOT)/release/$(APP)

view:
	@$(MAKE) --no-print-directory BUILD=debug $(VIEW_BIN)

$(BIN): $(OBJS) | $(BUILDDIR)
	@echo "LD  $@"
	$(CXX) $(APP_CXXFLAGS) -o $@ $(OBJS) $(APP_LDFLAGS) $(APP_LIBS)

$(VIEW_BIN): $(VIEW_LIB_OBJS) $(VIEW_DEBUG_OBJ) | $(BUILDDIR)
	@echo "LD  $@"
	$(CXX) $(APP_CXXFLAGS) -o $@ $(VIEW_LIB_OBJS) $(VIEW_DEBUG_OBJ) $(APP_LDFLAGS) $(VIEW_LIBS)

# debug.c is C++ (uses slop decode API + sokol).
$(VIEW_DEBUG_OBJ): $(SRCDIR)/debug.c
	@mkdir -p $(dir $@)
	@echo "CXX $<  (viewer)"
	$(CXX) $(APP_CXXFLAGS) $(APP_CPPFLAGS) -x c++ -c -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	$(CXX) $(APP_CXXFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(CC) $(APP_CFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(BUILDDIR)/vendors/%.o: vendors/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(CC) $(APP_CFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(BUILDDIR) $(TEST_BINDIR):
	@mkdir -p $@

clean:
	rm -rf $(BUILDROOT) $(TEST_BINDIR)

CURL := curl -fsSL -o

setup: setup-cgltf setup-cjson setup-stb setup-sokol setup-libs

setup-libs:
	@mkdir -p vendors/libs
	$(CURL) vendors/libs/testfw.h \
		https://raw.githubusercontent.com/mattiasgustavsson/libs/refs/heads/main/testfw.h
	$(CURL) vendors/libs/vecmath.h \
		https://raw.githubusercontent.com/mattiasgustavsson/libs/refs/heads/main/vecmath.h

setup-cgltf:
	@mkdir -p vendors/cgltf
	$(CURL) vendors/cgltf/cgltf.h \
		https://raw.githubusercontent.com/jkuhlmann/cgltf/refs/heads/master/cgltf.h
	$(CURL) vendors/cgltf/cgltf_write.h \
		https://raw.githubusercontent.com/jkuhlmann/cgltf/refs/heads/master/cgltf_write.h

setup-cjson:
	@mkdir -p vendors/cjson
	$(CURL) vendors/cjson/cJSON.c \
		https://raw.githubusercontent.com/DaveGamble/cJSON/refs/heads/master/cJSON.c
	$(CURL) vendors/cjson/cJSON.h \
		https://raw.githubusercontent.com/DaveGamble/cJSON/refs/heads/master/cJSON.h

setup-stb:
	@mkdir -p vendors/stb
	$(CURL) vendors/stb/stb_image.h \
		https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image.h
	$(CURL) vendors/stb/stb_image_write.h \
		https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image_write.h
	$(CURL) vendors/stb/stb_image_resize2.h \
		https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image_resize2.h

setup-sokol:
	@mkdir -p vendors/sokol vendors/sokol/util
	$(CURL) vendors/sokol/sokol_app.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_app.h
	$(CURL) vendors/sokol/sokol_args.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_args.h
	$(CURL) vendors/sokol/sokol_audio.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_audio.h
	$(CURL) vendors/sokol/sokol_fetch.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_fetch.h
	$(CURL) vendors/sokol/sokol_gfx.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_gfx.h
	$(CURL) vendors/sokol/sokol_glue.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_glue.h
	$(CURL) vendors/sokol/sokol_log.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_log.h
	$(CURL) vendors/sokol/sokol_time.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_time.h
	$(CURL) vendors/sokol/util/sokol_color.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_color.h
	$(CURL) vendors/sokol/util/sokol_letterbox.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_letterbox.h
	$(CURL) vendors/sokol/util/sokol_debugtext.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_debugtext.h
	$(CURL) vendors/sokol/util/sokol_framebuffer.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_framebuffer.h
	$(CURL) vendors/sokol/util/sokol_gl.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_gl.h
	$(CURL) vendors/sokol/util/sokol_fontstash.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_fontstash.h

# Run tests under tests/NNN-*.c or tests/NNN_*.c (gcc + Make; no CMake)
n ?=
s ?=
TEST_CFLAGS := -std=c99 -Wall -Wextra -g -D_DEFAULT_SOURCE
TEST_INCLUDES := -I. -Iinclude -Ivendors/libs
TEST_IMPL := tests/impl.c
ifeq ($(CROSS_COMPILE),)
TEST_CC ?= $(shell command -v musl-gcc 2>/dev/null || command -v x86_64-linux-musl-gcc 2>/dev/null || echo "$(CC)")
else
TEST_CC ?= $(CC)
endif

test:
	@mkdir -p $(TEST_BINDIR)
	@failed=0; \
	use_n=0; \
	start_s=-1; \
	if [ -n "$(n)" ]; then \
		use_n=1; \
	elif [ -n "$(s)" ]; then \
		start_s=$$(echo "$(s)" | sed 's/^0*//'); \
		[ -z "$$start_s" ] && start_s=0; \
	fi; \
	for src in tests/[0-9][0-9][0-9]_*.c tests/[0-9][0-9][0-9]-*.c; do \
		[ -f "$$src" ] || continue; \
		base=$$(basename "$$src" .c); \
		num=$$(echo "$$base" | sed 's/^\([0-9][0-9][0-9]\).*/\1/'); \
		if [ "$$use_n" -eq 1 ]; then \
			match=0; \
			rest="$(n)"; \
			rest=$$(echo "$$rest" | tr -d ' '); \
			while [ -n "$$rest" ]; do \
				part=$${rest%%,*}; \
				if [ "$$part" = "$$rest" ]; then rest=""; else rest=$${rest#*,}; fi; \
				[ -z "$$part" ] && continue; \
				pad=$$(printf '%03d' "$$part" 2>/dev/null || printf '%s' "$$part"); \
				if [ "$$num" = "$$pad" ]; then match=1; break; fi; \
			done; \
			[ "$$match" -eq 1 ] || continue; \
		elif [ "$$start_s" -ge 0 ] 2>/dev/null; then \
			num_i=$$(echo "$$num" | sed 's/^0*//'); \
			[ -z "$$num_i" ] && num_i=0; \
			[ "$$num_i" -ge "$$start_s" ] || continue; \
		fi; \
		echo "CC  $(TEST_BINDIR)/$$base  [$(TEST_CC)]"; \
		$(TEST_CC) $(TEST_CFLAGS) $(TEST_INCLUDES) -o $(TEST_BINDIR)/$$base $$src $(TEST_IMPL) || exit 1; \
		echo "RUN $(TEST_BINDIR)/$$base"; \
		$(TEST_BINDIR)/$$base || failed=1; \
	done; \
	if [ "$$failed" -ne 0 ]; then \
		echo "Some tests failed."; \
		exit 1; \
	fi; \
	echo "All selected tests passed."
