#version 330

in vec4 point;
in vec3 normal;

uniform mat4 camera;

out vec2 groundTexCoord;
out vec3 normal0;
out vec4 point0;

void main() {
    gl_Position = camera * point;
    groundTexCoord = point.xz;
    normal0=normal;
    point0=point;
}
