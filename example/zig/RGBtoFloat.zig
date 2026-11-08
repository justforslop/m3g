const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    var rgb: m3g.m3g_rgb = .{ .red = 255, .green = 128, .blue = 0 };
    var rgb_f: [3]f32 = undefined;
    m3g.m3g_rgb_to_float(&rgb, &rgb_f);
    std.debug.print("rgb(255,128,0) -> {any}\n", .{rgb_f});

    var rgba: m3g.m3g_rgba = .{ .red = 0, .green = 255, .blue = 0, .alpha = 128 };
    var rgba_f: [4]f32 = undefined;
    m3g.m3g_rgba_to_float(&rgba, &rgba_f);
    std.debug.print("rgba(0,255,0,128) -> {any}\n", .{rgba_f});
}
