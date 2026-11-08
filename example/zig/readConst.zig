const std = @import("std");
const m3g = @import("m3g.zig");

pub fn main() !void {
    std.debug.print("M3G_OK            = {}\n", .{m3g.M3G_OK});
    std.debug.print("M3G_ERR_FORMAT    = {}\n", .{m3g.M3G_ERR_FORMAT});
    std.debug.print("M3G_OBJ_MESH      = {}\n", .{m3g.M3G_OBJ_MESH});
    std.debug.print("M3G_ANIM_ALPHA    = {}\n", .{m3g.M3G_ANIM_ALPHA});
    std.debug.print("M3G_KF_SLERP      = {}\n", .{m3g.M3G_KF_SLERP});

    // Type name lookup is a real function call worth exercising.
    inline for (.{
        m3g.M3G_OBJ_HEADER,
        m3g.M3G_OBJ_GROUP,
        m3g.M3G_OBJ_MESH,
        m3g.M3G_OBJ_CAMERA,
        m3g.M3G_OBJ_WORLD,
        m3g.M3G_OBJ_EXTERNAL_REFERENCE,
    }) |ty| {
        const name = m3g.m3g_type_name(ty);
        std.debug.print("type {} -> '{s}'\n", .{ ty, std.mem.span(name) });
    }
}
