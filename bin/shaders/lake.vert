#version 330

in vec4 point;
in vec3 normal;
in float height;

uniform mat4 camera;
uniform vec3 position;
uniform float scale;

out vec3 normal0;
out vec4 point0;

void main() {
    gl_Position = camera * (point+vec4(0,height,0,0));
    normal0=normal;
    point0=point;
}
