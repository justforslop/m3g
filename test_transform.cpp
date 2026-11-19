#include "m3g.hpp"
#include <cstdio>

int main() {
    m3g::Transform t;
    t.postTranslate(1.f, 2.f, 3.f);
    const float *m = t.data();
    std::printf("m[3]=%.1f m[7]=%.1f m[11]=%.1f\n", m[3], m[7], m[11]);

    float x = 0, y = 0, z = 0, w = 1;
    float rx = m[0]*x + m[1]*y + m[2]*z + m[3]*w;
    float ry = m[4]*x + m[5]*y + m[6]*z + m[7]*w;
    float rz = m[8]*x + m[9]*y + m[10]*z + m[11]*w;
    std::printf("translate -> v = (%.1f, %.1f, %.1f)\n", rx, ry, rz);

    m3g::Transform s;
    s.postScale(2.f, 3.f, 4.f);
    const float *ms = s.data();
    std::printf("scale diag = (%.1f, %.1f, %.1f)\n", ms[0], ms[5], ms[10]);

    bool ok = (rx == 1.f && ry == 2.f && rz == 3.f &&
               ms[0] == 2.f && ms[5] == 3.f && ms[10] == 4.f);
    std::printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
