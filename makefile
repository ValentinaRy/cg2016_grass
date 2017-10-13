all:
	g++ -std=c++11 -O2 src/main.cpp src/Camera.cpp src/GL.cpp src/mat4.cpp src/Overall.cpp src/ShaderProgram.cpp src/Texture.cpp src/uvec3.cpp src/vec2.cpp src/vec3.cpp src/vec4.cpp -lGLEW -lGLU -lGL -lglut -lSOIL -lassimp -o bin/grass
