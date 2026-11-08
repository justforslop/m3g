const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    const cases = [_][3]f32{
        .{ 0, 0, 0 },
        .{ 1, 0, 0 },
        .{ 1, 1, 0 },
        .{ 3, 4, 0 },
        .{ 1, 2, 2 },
        .{ 2, 3, 6 },
        .{ -3, -4, 0 },
    };
    for (cases) |c| {
        const l = m3g.m3g_vec3_length(c[0], c[1], c[2]);
        std.debug.print("|({d},{d},{d})| = {d:.6}\n", .{ c[0], c[1], c[2], l });
    }
}
