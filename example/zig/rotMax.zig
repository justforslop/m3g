const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    const half_pi = std.math.pi / 2.0;

    // 90° about Z
    var r: [16]f32 = undefined;
    m3g.m3g_mat4_rotation_axis(half_pi, 0, 0, 1, &r);
    std.debug.print("rotZ(90°) = {any}\n", .{r});

    // Apply: v' = R * v. Column-major convention: v'[i] = sum_j r[j*4+i] * v[j].
    const v = [3]f32{ 1, 0, 0 };
    const x = r[0] * v[0] + r[4] * v[1] + r[8]  * v[2];
    const y = r[1] * v[0] + r[5] * v[1] + r[9]  * v[2];
    const z = r[2] * v[0] + r[6] * v[1] + r[10] * v[2];

    std.debug.print("R*(1,0,0) = ({d:.4}, {d:.4}, {d:.4})\n", .{ x, y, z });
}
