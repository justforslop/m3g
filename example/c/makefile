error:
	gcc example_errors.c -I../../include/  -L"../../build/lib/" -o example_error -lm3g -lm

handmade_file:
	gcc  example_handmade_file.c -I../../include/  -L"../../build/lib/" -o example_handmade_file -lm3g -lm

example_math:
	gcc example_math.c -I../../include/  -L"../../build/lib/" -o example_math -lm3g -lm


all: error handmade_file example_math

clear:
	rm example_error example_handmade_file example_math

