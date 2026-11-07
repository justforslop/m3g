walk:
	g++ example_decode_walk.cpp -I../../include/  -L"../../build/lib/" -o example_decode_walk -lm3g

convert:
	g++ example_convert_gltf.cpp -I../../include/  -L"../../build/lib/" -o example_convert_gltf -lm3g

build_scene:
	g++ example_build_scene.cpp -I../../include/  -L"../../build/lib/" -o example_build_scene -lm3g

all: walk build_scene convert

clean:
	rm example_build_scene example_convert_gltf example_decode_walk 
