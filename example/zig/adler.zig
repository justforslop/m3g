const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    // Must have installed a deflate backend that provides adler32.
    m3g.m3g_install_miniz_deflate_io();

    const adler0 = m3g.m3g_deflate_adler32(m3g.M3G_ADLER32_INIT, null, 0);
    std.debug.print("adler32(initial, empty) = {x}\n", .{adler0});

    const s = "hello";
    const adler = m3g.m3g_deflate_adler32(m3g.M3G_ADLER32_INIT, s.ptr, s.len);
    std.debug.print("adler32('hello')        = {x}\n", .{adler});

    // Expected 0x062c0215 for "hello" under the zlib adler32.
}
