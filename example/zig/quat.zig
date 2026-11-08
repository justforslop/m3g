const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    var q: [4]f32 = undefined;

    // 0° → identity quaternion (0,0,0,1)
    m3g.m3g_quat_from_axis_angle_deg(0, 1, 0, 0, &q);
    std.debug.print("q(0°, X) = {any}\n", .{q});

    // 180° about X → (sin90, 0, 0, cos90) = (1,0,0,0)
    m3g.m3g_quat_from_axis_angle_deg(180, 1, 0, 0, &q);
    std.debug.print("q(180°, X) = {any}\n", .{q});

    // 90° about Z → (0,0,sin45,cos45) ≈ (0,0,0.7071,0.7071)
    m3g.m3g_quat_from_axis_angle_deg(90, 0, 0, 1, &q);
    std.debug.print("q(90°, Z) = {any}\n", .{q});

    // Length should always be 1.
    const len = @sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    std.debug.print("|q| = {d:.6}\n", .{len});
}
