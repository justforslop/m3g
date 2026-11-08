const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    var trans: [16]f32 = undefined;
    m3g.m3g_mat4_translation(5, 6, 7, &trans);

    var scale: [16]f32 = undefined;
    m3g.m3g_mat4_scale(2, 3, 4, &scale);

    var out: [16]f32 = undefined;
    m3g.m3g_mat4_mul(&trans, &scale, &out);

    std.debug.print("T*S = {any}\n", .{out});

    // Verify expected diagonal: 2,3,4 and translation 5,6,7.
    std.debug.print("diag=({d},{d},{d}) trans=({d},{d},{d})\n", .{
        out[0], out[5], out[10], out[12], out[13], out[14],
    });
}
