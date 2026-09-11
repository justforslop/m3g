# m3g — M3G decoder / glTF converter (C++17)
#
# Build:
#   make            # viewer -> build/debug  (sokol)
#   make debug      # same
#   make view       # same as make debug
#   make release    # CLI    -> build/m3g
#   make lib        # static package library -> build/lib/libm3g.a
#   make install    # headers + libm3g.a + m3g.pc (+ optional CLI)
#   make uninstall
#   make clean
#   make doc         # Doxygen HTML -> docs/api/html (needs doxygen)
#
# Package install (DESTDIR + PREFIX, like autotools):
#   make setup && make lib BUILD=release
#   make install PREFIX=/usr/local DESTDIR=/tmp/stage
#   # pkg-config --cflags --libs m3g
#
# Bundle toggles (default ON; set =0 to omit vendor + adapter, like CMake):
#   make lib M3G_BUNDLE_MINIZ=0 M3G_BUNDLE_STB=0 M3G_BUNDLE_CJSON=0 M3G_BUNDLE_CGLTF=0
#   make lib M3G_WITH_EXPORT=0   # decode-only .a
#
# Zig (portable; Linux + Windows + cross — see build.zig / BUILD.md):
#   zig build -p build / lib / debug / view / release / test / setup / doc
#
# Viewer:
#   ./build/debug assets/90.m3g
#
# Object files live under build/obj/<config>/ so build/debug can be the binary.
#
# CMake (LSP compile_commands, optional ctest, find_package):
#   cmake -S . -B build && cmake --build build
#
# Vendored headers:
#   make setup   # or: zig build setup
.PHONY: all debug release view lib package install uninstall clean \
	setup setup-libs setup-cgltf setup-cjson setup-stb setup-sokol setup-miniz setup-imgui test doc

MINIZ_TAG := 3.1.2
MINIZ_URL := https://github.com/richgel999/miniz/releases/download/$(MINIZ_TAG)/miniz-$(MINIZ_TAG).zip

MINIZ_ARCHIVE := build/miniz-$(MINIZ_TAG).zip
MINIZ_DIR := vendors/miniz
MINIZ_STAMP := $(MINIZ_DIR)/.extracted

IMGUI_TAG := 1.92.9b
IMGUI_URL := https://github.com/ocornut/imgui/archive/refs/tags/v$(IMGUI_TAG).zip
IMGUI_ARCHIVE := build/imgui-$(IMGUI_TAG).zip
IMGUI_DIR := vendors/imgui
IMGUI_STAMP := $(IMGUI_DIR)/.extracted

APP      := m3g
VERSION  := 0.1.0
BUILDROOT := build
SRCDIR   := src
INCDIR   := include
TEST_BINDIR := tests/bin

# Install layout (make install / uninstall)
DESTDIR  ?=
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin
LIBDIR   := $(PREFIX)/lib
INCLUDEDIR := $(PREFIX)/include
PKGCONFIGDIR := $(LIBDIR)/pkgconfig
# Install CLI with the package? 1 = yes when build/m3g exists or after release
INSTALL_CLI ?= 1

CXX      ?= c++
CC       ?= gcc
AR       ?= ar
ARFLAGS  ?= rcs
BUILD    ?= debug
# Objects must not use build/debug/ — that path is the viewer executable.
OBJDIR   := $(BUILDROOT)/obj/$(BUILD)
LIBDIR_BUILD := $(BUILDROOT)/lib
INCDIR_BUILD := $(BUILDROOT)/include
PKGDIR_BUILD := $(BUILDROOT)/lib/pkgconfig

# ---- Package / backend knobs (mirror CMake M3G_BUNDLE_* / M3G_WITH_EXPORT) ----
# 1 = on, 0 = off. Full tree defaults match CMake top-level + Zig package.
M3G_WITH_EXPORT   ?= 1
M3G_BUNDLE_MINIZ  ?= 1
M3G_BUNDLE_STB    ?= 1
M3G_BUNDLE_CJSON  ?= 1
M3G_BUNDLE_CGLTF  ?= 1

# Isolate lib objects by bundle flags so toggling M3G_BUNDLE_* cannot reuse stale .o.
LIB_CFG := e$(M3G_WITH_EXPORT)m$(M3G_BUNDLE_MINIZ)s$(M3G_BUNDLE_STB)j$(M3G_BUNDLE_CJSON)g$(M3G_BUNDLE_CGLTF)
LIB_OBJDIR := $(BUILDROOT)/obj/lib-$(BUILD)-$(LIB_CFG)

M3G_BACKEND_DEFS :=
ifeq ($(M3G_BUNDLE_MINIZ),1)
M3G_BACKEND_DEFS += -DM3G_HAS_MINIZ_BACKEND=1
endif
ifeq ($(M3G_BUNDLE_STB),1)
M3G_BACKEND_DEFS += -DM3G_HAS_STB_BACKEND=1 -DM3G_IMPL_STB=1
endif
ifeq ($(M3G_BUNDLE_CJSON),1)
M3G_BACKEND_DEFS += -DM3G_HAS_CJSON_BACKEND=1
endif
ifeq ($(M3G_BUNDLE_CGLTF),1)
M3G_BACKEND_DEFS += -DM3G_HAS_CGLTF_BACKEND=1 -DM3G_IMPL_CGLTF=1
endif
ifeq ($(M3G_WITH_EXPORT),0)
M3G_BACKEND_DEFS += -DM3G_NO_EXPORT=1
endif

APP_CXXFLAGS_debug   := -std=c++17 -Wall -Wextra -O0 -g -D_DEFAULT_SOURCE -DAPP_NAME=\"$(APP)\" $(M3G_BACKEND_DEFS)
APP_CXXFLAGS_release := -std=c++17 -Wall -Wextra -Os -g0 -DNDEBUG -D_DEFAULT_SOURCE -DAPP_NAME=\"$(APP)\" $(M3G_BACKEND_DEFS)
APP_CFLAGS_debug     := -std=c99 -Wall -Wextra -O0 -g -D_DEFAULT_SOURCE $(M3G_BACKEND_DEFS)
APP_CFLAGS_release   := -std=c99 -Wall -Wextra -Os -g0 -DNDEBUG -D_DEFAULT_SOURCE $(M3G_BACKEND_DEFS)
APP_CXXFLAGS         := $(APP_CXXFLAGS_$(BUILD))
APP_CFLAGS           := $(APP_CFLAGS_$(BUILD))
APP_CPPFLAGS         := -I. -I$(INCDIR) -I$(SRCDIR) -Ivendors -Ivendors/cgltf -Ivendors/libs -Ivendors/miniz
VIEW_CPPFLAGS        := $(APP_CPPFLAGS) -Ivendors/imgui
APP_LDFLAGS          :=
APP_LIBS             := -lm
VIEW_LIBS            := $(APP_LIBS) -lGL -lX11 -lXi -lXcursor -ldl -lpthread
# Static .a + C++ global ctors (default backends): whole-archive keeps adapter TUs.
PKG_LIBS             := -Wl,--whole-archive -l$(APP) -Wl,--no-whole-archive -lm -lstdc++

# ---- Library package sources (no main.cpp / no viewer) ----
LIB_CXX_SRCS := $(SRCDIR)/decode/decoder.cpp
ifeq ($(M3G_WITH_EXPORT),1)
LIB_CXX_SRCS += \
	$(SRCDIR)/converter.cpp \
	$(SRCDIR)/export/gltf_exporter.cpp \
	$(SRCDIR)/gltf/gltf_writer.cpp \
	$(SRCDIR)/util/png_writer.cpp
endif
ifeq ($(M3G_BUNDLE_MINIZ),1)
LIB_CXX_SRCS += $(SRCDIR)/deflate_io_miniz.cpp
endif
ifeq ($(M3G_BUNDLE_STB),1)
LIB_CXX_SRCS += $(SRCDIR)/image_io_stb.cpp
endif
ifeq ($(M3G_BUNDLE_CJSON),1)
LIB_CXX_SRCS += $(SRCDIR)/json_io_cjson.cpp
endif
ifeq ($(M3G_BUNDLE_CGLTF),1)
LIB_CXX_SRCS += $(SRCDIR)/gltf_io_cgltf.cpp
endif

LIB_C_SRCS :=
ifeq ($(M3G_BUNDLE_STB),1)
LIB_C_SRCS += $(SRCDIR)/impl.c
else ifeq ($(M3G_BUNDLE_CGLTF),1)
LIB_C_SRCS += $(SRCDIR)/impl.c
endif
ifeq ($(M3G_BUNDLE_MINIZ),1)
LIB_C_SRCS += vendors/miniz/miniz.c
endif
ifeq ($(M3G_BUNDLE_CJSON),1)
LIB_C_SRCS += vendors/cjson/cJSON.c
endif

LIB_CXX_OBJS := $(patsubst $(SRCDIR)/%.cpp,$(LIB_OBJDIR)/%.o,$(filter $(SRCDIR)/%,$(LIB_CXX_SRCS)))
LIB_C_OBJS := $(patsubst $(SRCDIR)/%.c,$(LIB_OBJDIR)/%.o,$(filter $(SRCDIR)/%,$(LIB_C_SRCS)))
LIB_VENDOR_C_OBJS := $(patsubst vendors/%.c,$(LIB_OBJDIR)/vendors/%.o,$(filter vendors/%,$(LIB_C_SRCS)))
LIB_OBJS := $(LIB_CXX_OBJS) $(LIB_C_OBJS) $(LIB_VENDOR_C_OBJS)

STATIC_LIB := $(LIBDIR_BUILD)/lib$(APP).a
PC_BUILD   := $(PKGDIR_BUILD)/$(APP).pc

# Full CLI / viewer still compile the whole tree (historical Make behavior).
CXX_SRCS := $(shell find $(SRCDIR) -name '*.cpp' 2>/dev/null)
C_SRCS   := $(filter-out $(SRCDIR)/debug.c,$(shell find $(SRCDIR) -name '*.c' 2>/dev/null))
VENDOR_C_SRCS := vendors/cjson/cJSON.c vendors/miniz/miniz.c
CXX_OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(CXX_SRCS))
C_OBJS   := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(C_SRCS))
VENDOR_C_OBJS := $(patsubst vendors/%.c,$(OBJDIR)/vendors/%.o,$(VENDOR_C_SRCS))
OBJS := $(CXX_OBJS) $(C_OBJS) $(VENDOR_C_OBJS)
BIN  := $(BUILDROOT)/$(APP)
DEBUG_BIN := $(BUILDROOT)/debug
VIEW_LIB_OBJS := $(filter-out $(OBJDIR)/main.o,$(OBJS))
VIEW_DEBUG_OBJ := $(OBJDIR)/debug_view.o
IMGUI_SRCS := vendors/imgui/imgui.cpp vendors/imgui/imgui_draw.cpp vendors/imgui/imgui_tables.cpp vendors/imgui/imgui_widgets.cpp
IMGUI_OBJS := $(patsubst vendors/imgui/%.cpp,$(OBJDIR)/imgui/%.o,$(IMGUI_SRCS))

all: debug

debug view:
	@$(MAKE) --no-print-directory BUILD=debug $(DEBUG_BIN)

release:
	@$(MAKE) --no-print-directory BUILD=release $(BIN)

# Static package library + staged headers + pkg-config (build tree).
lib package:
	@$(MAKE) --no-print-directory BUILD=release $(STATIC_LIB) $(PC_BUILD) stage-headers

stage-headers: | $(INCDIR_BUILD)
	cp -f $(INCDIR)/m3g.h $(INCDIR)/m3g.hpp $(INCDIR_BUILD)/

$(STATIC_LIB): $(LIB_OBJS) | $(LIBDIR_BUILD)
	@echo "AR  $@"
	@rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

$(PC_BUILD): Makefile | $(PKGDIR_BUILD)
	@echo "PC  $@"
	@printf '%s\n' \
		'prefix=$(PREFIX)' \
		'exec_prefix=$${prefix}' \
		'libdir=$${exec_prefix}/lib' \
		'includedir=$${prefix}/include' \
		'' \
		'Name: m3g' \
		'Description: JSR-184 / M3G decode and glTF convert (C++17)' \
		'URL: https://github.com/justforslop/m3g' \
		'Version: $(VERSION)' \
		'Libs: -L$${libdir} $(PKG_LIBS)' \
		'Cflags: -I$${includedir} -std=c++17' \
		> $@

install: lib
	@echo "INSTALL -> $(DESTDIR)$(PREFIX)"
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(INCDIR)/m3g.h $(INCDIR)/m3g.hpp $(DESTDIR)$(INCLUDEDIR)/
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 $(STATIC_LIB) $(DESTDIR)$(LIBDIR)/lib$(APP).a
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	@# Rewrite prefix in the installed .pc (build copy used PREFIX already).
	sed -e 's|^prefix=.*|prefix=$(PREFIX)|' $(PC_BUILD) > $(DESTDIR)$(PKGCONFIGDIR)/$(APP).pc
	@chmod 644 $(DESTDIR)$(PKGCONFIGDIR)/$(APP).pc
ifeq ($(INSTALL_CLI),1)
	@$(MAKE) --no-print-directory BUILD=release $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(APP)
endif
	@echo "Installed m3g $(VERSION) (lib$(APP).a, headers, $(APP).pc$(if $(filter 1,$(INSTALL_CLI)),, ; CLI skipped))"

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/m3g.h $(DESTDIR)$(INCLUDEDIR)/m3g.hpp
	rm -f $(DESTDIR)$(LIBDIR)/lib$(APP).a
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(APP).pc
	rm -f $(DESTDIR)$(BINDIR)/$(APP)
	@echo "Uninstalled m3g from $(DESTDIR)$(PREFIX)"

$(BIN): $(OBJS) | $(BUILDROOT)
	@echo "LD  $@"
	$(CXX) $(APP_CXXFLAGS) -o $@ $(OBJS) $(APP_LDFLAGS) $(APP_LIBS)

$(DEBUG_BIN): $(VIEW_LIB_OBJS) $(VIEW_DEBUG_OBJ) $(IMGUI_OBJS) | $(BUILDROOT)
	@echo "LD  $@"
	$(CXX) $(APP_CXXFLAGS) -o $@ $(VIEW_LIB_OBJS) $(VIEW_DEBUG_OBJ) $(IMGUI_OBJS) $(APP_LDFLAGS) $(VIEW_LIBS)

# debug.c is C++ (uses m3g decode API + sokol + imgui).
$(VIEW_DEBUG_OBJ): $(SRCDIR)/debug.c
	@mkdir -p $(dir $@)
	@echo "CXX $<  (viewer)"
	$(CXX) $(APP_CXXFLAGS) $(VIEW_CPPFLAGS) -x c++ -c -o $@ $<

$(OBJDIR)/imgui/%.o: vendors/imgui/%.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	$(CXX) $(APP_CXXFLAGS) $(VIEW_CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	$(CXX) $(APP_CXXFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(CC) $(APP_CFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(OBJDIR)/vendors/%.o: vendors/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(CC) $(APP_CFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

# Package library objects (separate tree from CLI/viewer).
$(LIB_OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<  (lib)"
	$(CXX) $(APP_CXXFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(LIB_OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<  (lib)"
	$(CC) $(APP_CFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(LIB_OBJDIR)/vendors/%.o: vendors/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<  (lib)"
	$(CC) $(APP_CFLAGS) $(APP_CPPFLAGS) -c -o $@ $<

$(BUILDROOT) $(OBJDIR) $(LIB_OBJDIR) $(TEST_BINDIR) $(LIBDIR_BUILD) $(INCDIR_BUILD) $(PKGDIR_BUILD):
	@mkdir -p $@

clean:
	rm -rf $(BUILDROOT) $(TEST_BINDIR)

# HTML API docs from include/m3g.hpp (+ m3g.h). Requires doxygen on PATH.
DOXYFILE := Doxyfile
DOXY_OUT := docs/api/html/index.html

doc: $(DOXY_OUT)

$(DOXY_OUT): $(DOXYFILE) include/m3g.hpp include/m3g.h
	@command -v doxygen >/dev/null 2>&1 || { \
		echo "doxygen not found; install it to build API docs"; exit 1; }
	@mkdir -p docs/api
	@echo "DOXYGEN $(DOXYFILE) -> docs/api/html"
	doxygen $(DOXYFILE)
	@echo "Open docs/api/html/index.html"

CURL := curl -fsSL -o

setup: setup-cgltf setup-cjson setup-stb setup-sokol setup-libs setup-miniz setup-imgui

setup-libs:
	@mkdir -p vendors/libs
	$(CURL) vendors/libs/testfw.h \
		https://raw.githubusercontent.com/mattiasgustavsson/libs/refs/heads/main/testfw.h

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

# Only headers used by src/debug.c (viewer).
setup-sokol:
	@mkdir -p vendors/sokol vendors/sokol/util
	$(CURL) vendors/sokol/sokol_app.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_app.h
	$(CURL) vendors/sokol/sokol_gfx.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_gfx.h
	$(CURL) vendors/sokol/sokol_glue.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_glue.h
	$(CURL) vendors/sokol/sokol_log.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_log.h
	$(CURL) vendors/sokol/sokol_time.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_time.h
	$(CURL) vendors/sokol/util/sokol_gl.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_gl.h
	$(CURL) vendors/sokol/util/sokol_imgui.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_imgui.h
	$(CURL) vendors/sokol/util/sokol_gfx_imgui.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_gfx_imgui.h
	$(CURL) vendors/sokol/util/sokol_app_imgui.h \
		https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_app_imgui.h

setup-miniz: $(MINIZ_STAMP)

$(MINIZ_ARCHIVE):
	@mkdir -p $(dir $@)
	@curl -L --fail --output $@ $(MINIZ_URL)

$(MINIZ_STAMP): $(MINIZ_ARCHIVE)
	@rm -rf $(MINIZ_DIR)
	@mkdir -p $(MINIZ_DIR)
	@7z x $< -o$(MINIZ_DIR) -y
	@rm -rf $(MINIZ_DIR)/examples
	@rm -f $(MINIZ_DIR)/ChangeLog.md $(MINIZ_DIR)/readme.md
	@touch $@

setup-imgui: $(IMGUI_STAMP)

$(IMGUI_ARCHIVE):
	@mkdir -p $(dir $@)
	@curl -L --fail --output $@ $(IMGUI_URL)

$(IMGUI_STAMP): $(IMGUI_ARCHIVE)
	@rm -rf $(IMGUI_DIR)
	@mkdir -p $(IMGUI_DIR)
	@7z x $< -o$(IMGUI_DIR) -y
	@mv $(IMGUI_DIR)/imgui-$(IMGUI_TAG)/* $(IMGUI_DIR)/
	@rm -rf $(IMGUI_DIR)/imgui-$(IMGUI_TAG)
	@rm -rf $(IMGUI_DIR)/backends $(IMGUI_DIR)/docs $(IMGUI_DIR)/examples $(IMGUI_DIR)/misc
	@rm -f $(IMGUI_DIR)/imgui_demo.cpp
	@touch $@

# Run tests under tests/NNN_*.{c,cpp} or tests/NNN-*.{c,cpp}
n ?=
s ?=
TEST_CFLAGS := -std=c99 -Wall -Wextra -g -D_DEFAULT_SOURCE $(M3G_BACKEND_DEFS)
TEST_CXXFLAGS := -std=c++17 -Wall -Wextra -g -D_DEFAULT_SOURCE $(M3G_BACKEND_DEFS)
TEST_INCLUDES := -I. -Iinclude -Ivendors/libs
TEST_M3G_INCLUDES := -I. -Iinclude -Isrc -Ivendors -Ivendors/libs -Ivendors/cgltf -Ivendors/miniz
TEST_IMPL := tests/impl.c
# Library sources for C++ tests that need m3g (adapters auto-install backends).
TEST_M3G_SRCS := \
	src/converter.cpp \
	src/decode/decoder.cpp \
	src/deflate_io_miniz.cpp \
	src/export/gltf_exporter.cpp \
	src/gltf/gltf_writer.cpp \
	src/gltf_io_cgltf.cpp \
	src/image_io_stb.cpp \
	src/json_io_cjson.cpp \
	src/util/png_writer.cpp \
	src/impl.c \
	vendors/cjson/cJSON.c \
	vendors/miniz/miniz.c
TEST_M3G_LIBS := -lm
ifeq ($(CROSS_COMPILE),)
TEST_CC ?= $(shell command -v musl-gcc 2>/dev/null || command -v x86_64-linux-musl-gcc 2>/dev/null || echo "$(CC)")
else
TEST_CC ?= $(CC)
endif
TEST_CXX ?= $(CXX)

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
	for src in tests/[0-9][0-9][0-9]_*.c tests/[0-9][0-9][0-9]-*.c \
	           tests/[0-9][0-9][0-9]_*.cpp tests/[0-9][0-9][0-9]-*.cpp; do \
		[ -f "$$src" ] || continue; \
		case "$$src" in \
			*.cpp) base=$$(basename "$$src" .cpp); lang=cpp ;; \
			*)     base=$$(basename "$$src" .c);   lang=c ;; \
		esac; \
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
		if [ "$$lang" = cpp ]; then \
			echo "CXX $(TEST_BINDIR)/$$base  [$(TEST_CXX)]"; \
			$(TEST_CXX) $(TEST_CXXFLAGS) $(TEST_M3G_INCLUDES) -o $(TEST_BINDIR)/$$base \
				$$src $(TEST_IMPL) $(TEST_M3G_SRCS) $(TEST_M3G_LIBS) || exit 1; \
		else \
			echo "CC  $(TEST_BINDIR)/$$base  [$(TEST_CC)]"; \
			$(TEST_CC) $(TEST_CFLAGS) $(TEST_INCLUDES) -o $(TEST_BINDIR)/$$base $$src $(TEST_IMPL) || exit 1; \
		fi; \
		echo "RUN $(TEST_BINDIR)/$$base"; \
		$(TEST_BINDIR)/$$base || failed=1; \
	done; \
	if [ "$$failed" -ne 0 ]; then \
		echo "Some tests failed."; \
		exit 1; \
	fi; \
	echo "All selected tests passed."
