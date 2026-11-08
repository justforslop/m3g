const m3g = @import("m3g.zig");

const std = @import("std");

pub fn main() !void {
    
    var decoded: m3g.m3g_decoded = undefined;
    const path = "simple.m3g";

    const result = m3g.m3g_decode_file(path.ptr, null, &decoded);
    defer m3g.m3g_decoded_free(&decoded);

    if (result != m3g.M3G_OK) {
        std.debug.print("decode failed: {s}\n", .{m3g.m3g_last_error()});
        return;
    }
    const f=decoded.file;
    std.debug.print("objects: {}\n", .{f.object_count});
}
