#version 330

in vec2 TexCoord;
in vec4 point;
in vec3 normal;

uniform mat4 camera;
uniform vec3 position;
uniform float scale;

out vec2 TexCoord0;
out vec3 normal0;
out vec4 point0;

void main() {
    float normscale = 0.3;
    mat4 normscaleMatrix = mat4(1.0)*normscale;
    normscaleMatrix[3][3]=1;
    mat4 normpositionMatrix = mat4(1.0);
    normpositionMatrix[3][0] = 0.6;//position.x;
    normpositionMatrix[3][1] = 0;//position.y;
    normpositionMatrix[3][2] = 0.5;//position.z;
    vec4 normpoint = normpositionMatrix * normscaleMatrix *point;
    mat4 scaleMatrix = mat4(1.0)*scale;
    scaleMatrix[3][3]=1;
    mat4 rotateMatrix = mat4(1.0);
    mat4 positionMatrix = mat4(1.0);
    positionMatrix[3][0] = position.x;
    positionMatrix[3][1] = position.y;
    positionMatrix[3][2] = position.z;
    gl_Position = camera * (positionMatrix*scaleMatrix*rotateMatrix*normpoint);
    TexCoord0=TexCoord;
    normal0=(rotateMatrix*vec4(normal,1.0)).xyz;
    point0=positionMatrix*scaleMatrix*rotateMatrix*normpoint;
}
