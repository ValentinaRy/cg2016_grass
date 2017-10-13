#version 330

in vec4 point;
in vec3 normal;

uniform mat4 camera;
uniform vec3 position;
uniform float scale;

out vec3 normal0;
out vec4 point0;

void main() {
    mat4 scaleMatrix = mat4(1.0)*scale;
    scaleMatrix[3][3]=1;
    mat4 rotateMatrix = mat4(1.0);
    mat4 positionMatrix = mat4(1.0);
    positionMatrix[3][0] = position.x;
    positionMatrix[3][1] = position.y;
    positionMatrix[3][2] = position.z;
    gl_Position = camera * (positionMatrix*scaleMatrix*rotateMatrix*point);
    normal0=(rotateMatrix*vec4(normal,1.0)).xyz;
    point0=positionMatrix*scaleMatrix*rotateMatrix*point;
}
