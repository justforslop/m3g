const std = @import("std");
const builtin = @import("builtin");

// m3g — M3G decoder / glTF converter (C++17)
//
// Portable build driver (Linux + Windows + cross) and Zig package.
// Make remains available on POSIX.
//
//   zig build                 # viewer → <prefix>/debug
//   zig build debug|view      # same
//   zig build release         # CLI    → <prefix>/m3g
//   zig build lib             # static libm3g + public headers
//   zig build test            # unit tests (-Dn=1,2 / -Ds=1)
//   zig build setup           # fetch vendors (needs python3, 7z for zips)
//   zig build doc             # Doxygen → docs/api/html
//
// As a dependency (build.zig.zon → zig fetch):
//   const m3g = b.dependency("m3g", .{ .target = target, .optimize = optimize });
//   exe.linkLibrary(m3g.artifact("m3g"));
//   // public headers (m3g.h / m3g.hpp) come with the artifact
//
// Install next to Make/Ninja outputs:
//   zig build -p build
//   zig build -p build release
//   zig build -p build lib
//
// Windows / cross examples:
//   zig build -p build release -Dtarget=x86_64-windows-gnu
//   zig build -p build -Dtarget=x86_64-windows-gnu
//   zig build -p build release -Dtarget=aarch64-linux-gnu
//
// Default prefix is zig-out/ (binaries placed at prefix root, not prefix/bin).

const app_name = "m3g";

const lib_cpp_sources = [_][]const u8{
    "src/converter.cpp",
    "src/decode/decoder.cpp",
    "src/io_adapters_cxx.cpp",
    "src/export/gltf_exporter.cpp",
    "src/gltf/gltf_writer.cpp",
    "src/util/png_writer.cpp",
};

const lib_c_sources = [_][]const u8{
    "src/impl.c",
    "src/m3g_impl.c",
    "src/deflate_io_miniz.c",
    "src/image_io_stb.c",
    "src/json_io_cjson.c",
    "src/gltf_io_cgltf.c",
    "src/math_io_vecmath.c",
    "vendors/cjson/cJSON.c",
    "vendors/miniz/miniz.c",
};

// Match CMake M3G_BUNDLE_* defaults for the full package artifact.
const lib_backend_macros = [_]struct { []const u8, []const u8 }{
    .{ "M3G_HAS_MINIZ_BACKEND", "1" },
    .{ "M3G_HAS_STB_BACKEND", "1" },
    .{ "M3G_HAS_CJSON_BACKEND", "1" },
    .{ "M3G_HAS_CGLTF_BACKEND", "1" },
    .{ "M3G_HAS_VECMATH_BACKEND", "1" },
    .{ "M3G_IMPL_STB", "1" },
    .{ "M3G_IMPL_CGLTF", "1" },
};

const imgui_sources = [_][]const u8{
    "vendors/imgui/imgui.cpp",
    "vendors/imgui/imgui_draw.cpp",
    "vendors/imgui/imgui_tables.cpp",
    "vendors/imgui/imgui_widgets.cpp",
};

const miniz_tag = "3.1.2";
const imgui_tag = "1.92.9b";

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    // Host Python argv prefix (Windows often has `python` / `py -3`, not `python3`).
    const py_opt = b.option([]const u8, "py", "Python interpreter (default: auto-detect python3/python/py)");
    const py = resolvePython(b, py_opt);
    const n_opt = b.option([]const u8, "n", "Comma-separated test numbers (e.g. 1 or 2,3)");
    const s_opt = b.option([]const u8, "s", "Run tests from this number onward");

    const os = target.result.os.tag;
    const is_windows = os == .windows;
    const is_linux = os == .linux;

    const app_inc = [_][]const u8{ ".", "include", "src", "vendors", "vendors/cgltf", "vendors/libs", "vendors/miniz" };
    const view_inc = [_][]const u8{ ".", "include", "src", "vendors", "vendors/cgltf", "vendors/libs", "vendors/miniz", "vendors/imgui" };

    // Package / CLI: size-optimized when -Doptimize is left at default Debug (historical release).
    // Explicit -Doptimize=ReleaseFast|ReleaseSafe|… is honored for artifact "m3g".
    const lib_optimize: std.builtin.OptimizeMode = if (optimize == .Debug) .ReleaseSmall else optimize;
    const lib_cpp_flags = makeCppFlags(b, false, is_linux);
    const lib_c_flags = makeCFlags(b, false, is_linux);
    const view_cpp_flags = makeCppFlags(b, true, is_linux);
    const view_c_flags = makeCFlags(b, true, is_linux);

    // ---- Package library (artifact "m3g") ----
    // Static C++17 library: decode + convert + default IO adapters (miniz/stb/cJSON/cgltf).
    // Consumers: dep.artifact("m3g") then linkLibrary; public headers travel with the artifact.
    const lib_mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = lib_optimize,
        .link_libc = true,
        .link_libcpp = true,
    });
    addIncludes(lib_mod, b, &app_inc);
    if (is_linux) lib_mod.addCMacro("_DEFAULT_SOURCE", "1");
    for (lib_backend_macros) |m| lib_mod.addCMacro(m[0], m[1]);
    addCppSources(lib_mod, b, &lib_cpp_sources, lib_cpp_flags);
    addCSources(lib_mod, b, &lib_c_sources, lib_c_flags);
    if (!is_windows) lib_mod.linkSystemLibrary("m", .{});

    const lib = b.addLibrary(.{
        .name = app_name,
        .linkage = .static,
        .root_module = lib_mod,
        .version = .{ .major = 0, .minor = 1, .patch = 0 },
    });
    lib.installHeader(b.path("include/m3g.h"), "m3g.h");
    lib.installHeader(b.path("include/m3g.hpp"), "m3g.hpp");

    // Must hang off the install step so b.dependency(...).artifact("m3g") can resolve it.
    // Named "m3g" — do not install the CLI (also named m3g) on this step (ambiguous).
    const install_lib = b.addInstallArtifact(lib, .{});
    b.getInstallStep().dependOn(&install_lib.step);
    const lib_step = b.step("lib", "Build and install static libm3g + public headers");
    lib_step.dependOn(&install_lib.step);

    // ---- CLI ----
    const cli_mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = lib_optimize,
        .link_libc = true,
        .link_libcpp = true,
    });
    addIncludes(cli_mod, b, &app_inc);
    cli_mod.addCMacro("APP_NAME", b.fmt("\"{s}\"", .{app_name}));
    if (is_linux) cli_mod.addCMacro("_DEFAULT_SOURCE", "1");
    cli_mod.addCSourceFile(.{ .file = b.path("src/main.cpp"), .flags = lib_cpp_flags, .language = .cpp });
    cli_mod.linkLibrary(lib);
    if (!is_windows) cli_mod.linkSystemLibrary("m", .{});

    const cli = b.addExecutable(.{
        .name = app_name,
        .root_module = cli_mod,
    });
    const install_cli = b.addInstallArtifact(cli, .{
        .dest_dir = .{ .override = .prefix },
    });
    const release_step = b.step("release", "Build CLI converter (m3g)");
    release_step.dependOn(&install_cli.step);

    // ---- Viewer ----
    // Own Debug compile of library sources (prior behavior: -O0 -g for viewer).
    const view_mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = .Debug,
        .link_libc = true,
        .link_libcpp = true,
    });
    addIncludes(view_mod, b, &view_inc);
    view_mod.addCMacro("APP_NAME", b.fmt("\"{s}\"", .{app_name}));
    if (is_linux) view_mod.addCMacro("_DEFAULT_SOURCE", "1");
    for (lib_backend_macros) |m| view_mod.addCMacro(m[0], m[1]);
    addCppSources(view_mod, b, &lib_cpp_sources, view_cpp_flags);
    addCSources(view_mod, b, &lib_c_sources, view_c_flags);
    view_mod.addCSourceFile(.{
        .file = b.path("src/debug.cpp"),
        .flags = view_cpp_flags,
        .language = .cpp,
    });
    addCppSources(view_mod, b, &imgui_sources, view_cpp_flags);
    if (!is_windows) view_mod.linkSystemLibrary("m", .{});
    linkViewer(view_mod, os);

    const viewer = b.addExecutable(.{
        .name = "debug",
        .root_module = view_mod,
    });
    const install_view = b.addInstallArtifact(viewer, .{
        .dest_dir = .{ .override = .prefix },
    });
    const debug_step = b.step("debug", "Build Sokol + ImGui viewer");
    debug_step.dependOn(&install_view.step);
    const view_step = b.step("view", "Alias for debug (viewer)");
    view_step.dependOn(debug_step);

    // Default install = viewer (matches Make/Ninja `all`) + package lib for artifact()
    b.getInstallStep().dependOn(&install_view.step);

    addSetupSteps(b, py);
    addDocStep(b, py);
    addTestStep(b, py, n_opt, s_opt);
}

/// Resolve a host Python invocation as an argv prefix.
/// Windows: prefer `python.exe`, then `python3.exe`, then `py.exe -3`.
/// POSIX: prefer `python3`, then `python`.
/// `-Dpy=...` overrides (use `-Dpy=py` for the Windows launcher → `py -3`).
fn resolvePython(b: *std.Build, override: ?[]const u8) []const []const u8 {
    if (override) |user| {
        if (std.mem.eql(u8, user, "py") or std.mem.eql(u8, user, "py.exe")) {
            return b.dupeStrings(&.{ user, "-3" });
        }
        return b.dupeStrings(&.{user});
    }

    const host_win = builtin.os.tag == .windows;
    const names: []const []const u8 = if (host_win)
        &.{ "python.exe", "python3.exe", "py.exe", "python", "python3", "py" }
    else
        &.{ "python3", "python" };

    if (b.findProgram(names, &.{})) |path| {
        const base = std.fs.path.basename(path);
        const is_py_launcher = std.mem.eql(u8, base, "py.exe") or
            std.mem.eql(u8, base, "py") or
            std.ascii.eqlIgnoreCase(base, "py.exe");
        if (is_py_launcher) {
            return b.dupeStrings(&.{ path, "-3" });
        }
        return b.dupeStrings(&.{path});
    } else |_| {}

    // Last resort: still emit a sensible default so the error message is clear.
    if (host_win) return b.dupeStrings(&.{"python"});
    return b.dupeStrings(&.{"python3"});
}

fn addPyCommand(b: *std.Build, py: []const []const u8) *std.Build.Step.Run {
    return b.addSystemCommand(py);
}

fn makeCppFlags(b: *std.Build, debug: bool, is_linux: bool) []const []const u8 {
    var list: std.ArrayList([]const u8) = .empty;
    list.appendSlice(b.allocator, &.{ "-std=c++17", "-Wall", "-Wextra" }) catch @panic("OOM");
    if (debug) {
        list.appendSlice(b.allocator, &.{ "-O0", "-g" }) catch @panic("OOM");
    } else {
        list.appendSlice(b.allocator, &.{ "-Os", "-g0", "-DNDEBUG" }) catch @panic("OOM");
    }
    if (is_linux) list.append(b.allocator, "-D_DEFAULT_SOURCE") catch @panic("OOM");
    return list.toOwnedSlice(b.allocator) catch @panic("OOM");
}

fn makeCFlags(b: *std.Build, debug: bool, is_linux: bool) []const []const u8 {
    var list: std.ArrayList([]const u8) = .empty;
    list.appendSlice(b.allocator, &.{ "-std=c99", "-Wall", "-Wextra" }) catch @panic("OOM");
    if (debug) {
        list.appendSlice(b.allocator, &.{ "-O0", "-g" }) catch @panic("OOM");
    } else {
        list.appendSlice(b.allocator, &.{ "-Os", "-g0", "-DNDEBUG" }) catch @panic("OOM");
    }
    if (is_linux) list.append(b.allocator, "-D_DEFAULT_SOURCE") catch @panic("OOM");
    return list.toOwnedSlice(b.allocator) catch @panic("OOM");
}

fn addIncludes(mod: *std.Build.Module, b: *std.Build, paths: []const []const u8) void {
    for (paths) |p| mod.addIncludePath(b.path(p));
}

fn addCppSources(mod: *std.Build.Module, b: *std.Build, files: []const []const u8, flags: []const []const u8) void {
    for (files) |f| {
        mod.addCSourceFile(.{ .file = b.path(f), .flags = flags, .language = .cpp });
    }
}

fn addCSources(mod: *std.Build.Module, b: *std.Build, files: []const []const u8, flags: []const []const u8) void {
    for (files) |f| {
        mod.addCSourceFile(.{ .file = b.path(f), .flags = flags, .language = .c });
    }
}

fn linkViewer(mod: *std.Build.Module, os: std.Target.Os.Tag) void {
    switch (os) {
        .windows => {
            // SOKOL_D3D11 (see src/debug.cpp) — not WGL/opengl32.
            mod.linkSystemLibrary("d3d11", .{});
            mod.linkSystemLibrary("dxgi", .{});
            mod.linkSystemLibrary("user32", .{});
            mod.linkSystemLibrary("gdi32", .{});
            mod.linkSystemLibrary("shell32", .{});
            mod.linkSystemLibrary("kernel32", .{});
        },
        else => {
            // Linux / *BSD: Sokol GLCORE + X11
            mod.linkSystemLibrary("GL", .{});
            mod.linkSystemLibrary("X11", .{});
            mod.linkSystemLibrary("Xi", .{});
            mod.linkSystemLibrary("Xcursor", .{});
            mod.linkSystemLibrary("dl", .{});
            mod.linkSystemLibrary("pthread", .{});
        },
    }
}

fn addDocStep(b: *std.Build, py: []const []const u8) void {
    const doxygen = addPyCommand(b, py);
    doxygen.addArgs(&.{
        "-c",
        \\import shutil,sys,os,subprocess
        \\exe=shutil.which('doxygen') or sys.exit('doxygen not found on PATH')
        \\os.makedirs('docs/api', exist_ok=True)
        \\subprocess.check_call([exe, 'Doxyfile'])
        \\print('Open docs/api/html/index.html')
        ,
    });
    doxygen.setCwd(b.path("."));
    const doc_step = b.step("doc", "Generate Doxygen HTML API docs");
    doc_step.dependOn(&doxygen.step);
}

fn addSetupSteps(b: *std.Build, py: []const []const u8) void {
    const setup = b.step("setup", "Fetch all vendored third-party sources");

    const Curl = struct {
        fn stamp(b2: *std.Build, py2: []const []const u8, name: []const u8, url: []const u8, dest: []const u8) *std.Build.Step {
            const stamp_path = b2.fmt("build/setup/{s}", .{name});
            const mkdir = addPyCommand(b2, py2);
            mkdir.addArgs(&.{
                "-c",
                "import os,sys; os.makedirs(os.path.dirname(sys.argv[1]), exist_ok=True); os.makedirs(os.path.dirname(sys.argv[2]) or '.', exist_ok=True)",
                stamp_path,
                dest,
            });
            mkdir.setCwd(b2.path("."));

            const cmd = addPyCommand(b2, py2);
            cmd.addFileArg(b2.path("scripts/curl_stamp.py"));
            cmd.addArgs(&.{ url, dest, stamp_path });
            cmd.setCwd(b2.path("."));
            cmd.step.dependOn(&mkdir.step);
            return &cmd.step;
        }
    };

    const setup_libs = b.step("setup-libs", "Fetch testfw.h");
    setup_libs.dependOn(Curl.stamp(b, py, "libs-testfw.h", "https://raw.githubusercontent.com/mattiasgustavsson/libs/refs/heads/main/testfw.h", "vendors/libs/testfw.h"));

    const setup_cgltf = b.step("setup-cgltf", "Fetch cgltf headers");
    setup_cgltf.dependOn(Curl.stamp(b, py, "cgltf.h", "https://raw.githubusercontent.com/jkuhlmann/cgltf/refs/heads/master/cgltf.h", "vendors/cgltf/cgltf.h"));
    setup_cgltf.dependOn(Curl.stamp(b, py, "cgltf_write.h", "https://raw.githubusercontent.com/jkuhlmann/cgltf/refs/heads/master/cgltf_write.h", "vendors/cgltf/cgltf_write.h"));

    const setup_cjson = b.step("setup-cjson", "Fetch cJSON");
    setup_cjson.dependOn(Curl.stamp(b, py, "cJSON.c", "https://raw.githubusercontent.com/DaveGamble/cJSON/refs/heads/master/cJSON.c", "vendors/cjson/cJSON.c"));
    setup_cjson.dependOn(Curl.stamp(b, py, "cJSON.h", "https://raw.githubusercontent.com/DaveGamble/cJSON/refs/heads/master/cJSON.h", "vendors/cjson/cJSON.h"));

    const setup_stb = b.step("setup-stb", "Fetch stb image headers");
    setup_stb.dependOn(Curl.stamp(b, py, "stb_image.h", "https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image.h", "vendors/stb/stb_image.h"));
    setup_stb.dependOn(Curl.stamp(b, py, "stb_image_write.h", "https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image_write.h", "vendors/stb/stb_image_write.h"));

    const sokol_files = [_]struct { name: []const u8, dest: []const u8, url: []const u8 }{
        .{ .name = "sokol_app.h", .dest = "vendors/sokol/sokol_app.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_app.h" },
        .{ .name = "sokol_gfx.h", .dest = "vendors/sokol/sokol_gfx.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_gfx.h" },
        .{ .name = "sokol_glue.h", .dest = "vendors/sokol/sokol_glue.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_glue.h" },
        .{ .name = "sokol_log.h", .dest = "vendors/sokol/sokol_log.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_log.h" },
        .{ .name = "sokol_time.h", .dest = "vendors/sokol/sokol_time.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/sokol_time.h" },
        .{ .name = "sokol_gl.h", .dest = "vendors/sokol/util/sokol_gl.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_gl.h" },
        .{ .name = "sokol_imgui.h", .dest = "vendors/sokol/util/sokol_imgui.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_imgui.h" },
        .{ .name = "sokol_gfx_imgui.h", .dest = "vendors/sokol/util/sokol_gfx_imgui.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_gfx_imgui.h" },
        .{ .name = "sokol_app_imgui.h", .dest = "vendors/sokol/util/sokol_app_imgui.h", .url = "https://raw.githubusercontent.com/floooh/sokol/refs/heads/master/util/sokol_app_imgui.h" },
    };
    const setup_sokol = b.step("setup-sokol", "Fetch Sokol headers (viewer)");
    for (sokol_files) |sf| {
        setup_sokol.dependOn(Curl.stamp(b, py, sf.name, sf.url, sf.dest));
    }

    const mkdir_build = addPyCommand(b, py);
    mkdir_build.addArgs(&.{ "-c", "import os; os.makedirs('build', exist_ok=True)" });
    mkdir_build.setCwd(b.path("."));

    const miniz_url = b.fmt("https://github.com/richgel999/miniz/releases/download/{s}/miniz-{s}.zip", .{ miniz_tag, miniz_tag });
    const miniz_archive = b.fmt("build/miniz-{s}.zip", .{miniz_tag});
    const curl_miniz = addPyCommand(b, py);
    curl_miniz.addFileArg(b.path("scripts/curl_zip.py"));
    curl_miniz.addArgs(&.{ miniz_url, miniz_archive });
    curl_miniz.setCwd(b.path("."));
    curl_miniz.step.dependOn(&mkdir_build.step);
    const extract_miniz = addPyCommand(b, py);
    extract_miniz.addFileArg(b.path("scripts/extract_vendor.py"));
    extract_miniz.addArgs(&.{ "miniz", miniz_archive, "vendors/miniz", "vendors/miniz/.extracted" });
    extract_miniz.setCwd(b.path("."));
    extract_miniz.step.dependOn(&curl_miniz.step);
    const setup_miniz = b.step("setup-miniz", "Fetch and extract miniz");
    setup_miniz.dependOn(&extract_miniz.step);

    const imgui_url = b.fmt("https://github.com/ocornut/imgui/archive/refs/tags/v{s}.zip", .{imgui_tag});
    const imgui_archive = b.fmt("build/imgui-{s}.zip", .{imgui_tag});
    const curl_imgui = addPyCommand(b, py);
    curl_imgui.addFileArg(b.path("scripts/curl_zip.py"));
    curl_imgui.addArgs(&.{ imgui_url, imgui_archive });
    curl_imgui.setCwd(b.path("."));
    curl_imgui.step.dependOn(&mkdir_build.step);
    const extract_imgui = addPyCommand(b, py);
    extract_imgui.addFileArg(b.path("scripts/extract_vendor.py"));
    extract_imgui.addArgs(&.{ "imgui", imgui_archive, "vendors/imgui", imgui_tag, "vendors/imgui/.extracted" });
    extract_imgui.setCwd(b.path("."));
    extract_imgui.step.dependOn(&curl_imgui.step);
    const setup_imgui = b.step("setup-imgui", "Fetch and extract Dear ImGui");
    setup_imgui.dependOn(&extract_imgui.step);

    setup.dependOn(setup_cgltf);
    setup.dependOn(setup_cjson);
    setup.dependOn(setup_stb);
    setup.dependOn(setup_sokol);
    setup.dependOn(setup_libs);
    setup.dependOn(setup_miniz);
    setup.dependOn(setup_imgui);
}

fn addTestStep(b: *std.Build, py: []const []const u8, n_opt: ?[]const u8, s_opt: ?[]const u8) void {
    const run = addPyCommand(b, py);
    run.addFileArg(b.path("scripts/run_tests.py"));
    run.setCwd(b.path("."));

    // Drive tests with zig cc / zig c++ so Windows does not need a separate MinGW install.
    run.setEnvironmentVariable("TEST_CC", b.graph.zig_exe);
    run.setEnvironmentVariable("TEST_CXX", b.graph.zig_exe);
    run.setEnvironmentVariable("TEST_USE_ZIG", "1");
    // Empty default on Windows avoids a failing `-lm` with some toolchains; override via env.
    if (builtin.os.tag == .windows) {
        run.setEnvironmentVariable("TEST_M3G_LIBS", "");
    }
    if (n_opt) |n| run.setEnvironmentVariable("n", n);
    if (s_opt) |s| run.setEnvironmentVariable("s", s);

    const test_step = b.step("test", "Compile and run unit tests (-Dn= / -Ds= filters)");
    test_step.dependOn(&run.step);
}
