const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    // Build T * R * S
    var t: [16]f32 = undefined;
    m3g.m3g_mat4_translation(10, 20, 30, &t);

    var r: [16]f32 = undefined;
    m3g.m3g_mat4_rotation_axis(std.math.pi / 4.0, 0, 1, 0, &r);

    var s: [16]f32 = undefined;
    m3g.m3g_mat4_scale(2, 2, 2, &s);

    var ts: [16]f32 = undefined;
    m3g.m3g_mat4_mul(&t, &r, &ts);
    var trs: [16]f32 = undefined;
    m3g.m3g_mat4_mul(&ts, &s, &trs);

    // Now decompose.
    var out_t: [3]f32 = undefined;
    var out_q: [4]f32 = undefined;
    var out_s: [3]f32 = undefined;
    m3g.m3g_decompose_trs(&trs, &out_t, &out_q, &out_s);

    std.debug.print("T = {any}\n", .{out_t});
    std.debug.print("Q = {any}\n", .{out_q});
    std.debug.print("S = {any}\n", .{out_s});
}
