hello:
	zig build-exe hellom3g.zig   -I../../src   -L../../build/lib -lm3g -lc
smoke:
	zig build-exe smoke.zig -I../../src   -L../../build/lib -lm3g -lc
ep:
	zig build-exe epsilon.zig -I../../src   -L../../build/lib -lm3g -lc
rotMax:
	zig build-exe rotMax.zig -I../../src   -L../../build/lib -lm3g -lc
quat:
	zig build-exe quat.zig -I../../src   -L../../build/lib -lm3g -lc
trsMat:
	zig build-exe trsMat.zig -I../../src   -L../../build/lib -lm3g -lc
Vec3Len:
	zig build-exe Vec3Len.zig -I../../src   -L../../build/lib -lm3g -lc
readConst:
	zig build-exe readConst.zig -I../../src   -L../../build/lib -lm3g -lc
MatMeta:
	zig build-exe MatMeta.zig -I../../src   -L../../build/lib -lm3g -lc
RGBtoFloat:
	zig build-exe RGBtoFloat.zig -I../../src   -L../../build/lib -lm3g -lc
adler:
	zig build-exe adler.zig -I../../src   -L../../build/lib -lm3g -lc

clean:
	rm hellom3g  smoke epsilon rotMax quat trsMat Vec3Len readConst MatMeta RGBtoFloat adler

run_a:
	 ./hellom3g && ./smoke && ./rotMax && ./quat && ./trsMat && ./Vec3Len && ./readConst && ./MatMeta && ./RGBtoFloat && ./adler

all: hello smoke rotMax quat trsMat Vec3Len readConst MatMeta RGBtoFloat adler ep
