#!/usr/bin/env sh
# Portable unit-test runner (Make-free). Used by: ninja test
# Filters (optional): n=1,2  or  s=10
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

TEST_BINDIR=${TEST_BINDIR:-tests/bin}
TEST_CC=${TEST_CC:-${test_cc:-gcc}}
TEST_CXX=${TEST_CXX:-${cxx:-c++}}
TEST_CFLAGS=${TEST_CFLAGS:--std=c99 -Wall -Wextra -g -D_DEFAULT_SOURCE}
TEST_CXXFLAGS=${TEST_CXXFLAGS:--std=c++17 -Wall -Wextra -g -D_DEFAULT_SOURCE}
TEST_INCLUDES="-I. -Iinclude -Ivendors/libs"
TEST_M3G_INCLUDES="-I. -Iinclude -Isrc -Ivendors -Ivendors/libs -Ivendors/cgltf -Ivendors/miniz"
TEST_IMPL=tests/impl.c
TEST_M3G_LIBS=${TEST_M3G_LIBS:--lm}

TEST_M3G_SRCS="
	src/converter.cpp
	src/decode/decoder.cpp
	src/deflate_io_miniz.cpp
	src/export/gltf_exporter.cpp
	src/gltf/gltf_writer.cpp
	src/gltf_io_cgltf.cpp
	src/image_io_stb.cpp
	src/json_io_cjson.cpp
	src/util/png_writer.cpp
	src/impl.c
	vendors/cjson/cJSON.c
	vendors/miniz/miniz.c
"

mkdir -p "$TEST_BINDIR"

n=${n:-}
s=${s:-}
failed=0
use_n=0
start_s=-1

if [ -n "$n" ]; then
	use_n=1
elif [ -n "$s" ]; then
	start_s=$(echo "$s" | sed 's/^0*//')
	[ -z "$start_s" ] && start_s=0
fi

# shellcheck disable=SC2086
for src in tests/[0-9][0-9][0-9]_*.c tests/[0-9][0-9][0-9]-*.c \
	tests/[0-9][0-9][0-9]_*.cpp tests/[0-9][0-9][0-9]-*.cpp; do
	[ -f "$src" ] || continue
	case "$src" in
		*.cpp) base=$(basename "$src" .cpp); lang=cpp ;;
		*)     base=$(basename "$src" .c);   lang=c ;;
	esac
	num=$(echo "$base" | sed 's/^\([0-9][0-9][0-9]\).*/\1/')

	if [ "$use_n" -eq 1 ]; then
		match=0
		rest=$(echo "$n" | tr -d ' ')
		while [ -n "$rest" ]; do
			part=${rest%%,*}
			if [ "$part" = "$rest" ]; then rest=""; else rest=${rest#*,}; fi
			[ -z "$part" ] && continue
			pad=$(printf '%03d' "$part" 2>/dev/null || printf '%s' "$part")
			if [ "$num" = "$pad" ]; then match=1; break; fi
		done
		[ "$match" -eq 1 ] || continue
	elif [ "$start_s" -ge 0 ] 2>/dev/null; then
		num_i=$(echo "$num" | sed 's/^0*//')
		[ -z "$num_i" ] && num_i=0
		[ "$num_i" -ge "$start_s" ] || continue
	fi

	exe_out="$TEST_BINDIR/$base"
	case "$(uname -s 2>/dev/null || echo unknown)" in
		MINGW*|MSYS*|CYGWIN*) exe_out="$exe_out.exe" ;;
	esac

	if [ "$lang" = cpp ]; then
		echo "CXX $exe_out  [$TEST_CXX]"
		# shellcheck disable=SC2086
		$TEST_CXX $TEST_CXXFLAGS $TEST_M3G_INCLUDES -o "$exe_out" \
			"$src" "$TEST_IMPL" $TEST_M3G_SRCS $TEST_M3G_LIBS || exit 1
	else
		echo "CC  $exe_out  [$TEST_CC]"
		# shellcheck disable=SC2086
		$TEST_CC $TEST_CFLAGS $TEST_INCLUDES -o "$exe_out" "$src" "$TEST_IMPL" || exit 1
	fi
	echo "RUN $exe_out"
	"$exe_out" || failed=1
done

if [ "$failed" -ne 0 ]; then
	echo "Some tests failed."
	exit 1
fi
echo "All selected tests passed."
