const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    // Build a node meta with an explicit component transform.
    var node: m3g.m3g_node_meta = .{};

    // Mark translation+scale+rotation as present.
    node.transformable.component.present = 1;
    node.transformable.component.translation = .{ 1.0, 2.0, 3.0 };
    node.transformable.component.scale = .{ 2.0, 2.0, 2.0 };
    node.transformable.component.orientation_angle = std.math.pi / 2.0;
    node.transformable.component.orientation_axis = .{ 0.0, 1.0, 0.0 };

    var m: [16]f32 = undefined;
    m3g.m3g_node_matrix(&node, &m);
    std.debug.print("node matrix = {any}\n", .{m});

    // No component, no general transform → identity.
    var empty: m3g.m3g_node_meta = .{};
    var m2: [16]f32 = undefined;
    m3g.m3g_node_matrix(&empty, &m2);
    std.debug.print("empty node matrix = {any}\n", .{m2});
}
