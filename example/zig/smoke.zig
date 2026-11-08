const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    var m: [16]f32 = undefined;
    m3g.m3g_mat4_identity(&m);
    std.debug.print("identity: {any}\n", .{m});

    var t: [16]f32 = undefined;
    m3g.m3g_mat4_translation(1, 2, 3, &t);
    std.debug.print("translation: {any}\n", .{t});

    var q: [4]f32 = undefined;
    m3g.m3g_quat_from_axis_angle_deg(90, 0, 1, 0, &q);
    std.debug.print("quat(90° around Y): {any}\n", .{q});

    std.debug.print("vec3_length(3,4,0) = {d}\n", .{ m3g.m3g_vec3_length(3, 4, 0) });
}
