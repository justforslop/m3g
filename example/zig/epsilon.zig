const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    var id: [16]f32 = undefined;
    m3g.m3g_mat4_identity(&id);
    std.debug.print("is_identity(id, 1e-5) = {}\n", .{
        m3g.m3g_mat4_is_identity(&id, 1e-5),
    });

    // Perturb one element just above epsilon.
    var bad = id;
    bad[3] = 0.001;
    std.debug.print("is_identity(bad, 1e-5) = {}\n", .{
        m3g.m3g_mat4_is_identity(&bad, 1e-5),
    });

    // Perturb below epsilon.
    var near = id;
    near[3] = 0.000001;
    std.debug.print("is_identity(near, 1e-5) = {}\n", .{
        m3g.m3g_mat4_is_identity(&near, 1e-5),
    });
}
