#version 330

in vec4 point;
in vec3 position;
in vec4 variance;
in vec3 normal;
in vec3 grassScale;
in float alpha;

uniform mat4 camera;

out vec2 grassTexCoord;
out vec3 normal0;
out vec4 point0;
out float height;

void main() {
    float scale = 0.025; //0.025
    mat4 scaleMatrix = mat4(
                        scale/10*grassScale.x, 0.0, 0.0, 0.0,
                        0.0, scale*grassScale.y, 0.0, 0.0,
                        0.0, 0.0, scale/10*grassScale.z, 0.0,
                        0.0, 0.0, 0.0, 1.0
    );//mat4(1.0);
    //scaleMatrix[0][0] = 0.01;
    //scaleMatrix[1][1] = 0.1;
    mat4 rotateMatrix = mat4(
                        cos(alpha), 0.0, -sin(alpha), 0.0,
                        0.0, 1.0, 0.0, 0.0,
                        sin(alpha), 0.0, cos(alpha), 0.0,
                        0.0, 0.0, 0.0, 1.0
    );

    mat4 positionMatrix = mat4(1.0);
    positionMatrix[3][0] = position.x;
    positionMatrix[3][1] = position.y;
    positionMatrix[3][2] = position.z;

	gl_Position = camera * (positionMatrix * scaleMatrix * (rotateMatrix * point
        + vec4(variance.x*(1+8*point.y),(-1+sqrt(1-variance.x*variance.x)),0,0)*point.y/(2-point.y)));
    grassTexCoord = point.xy;
    normal0=(rotateMatrix*vec4(normal,1.0)).xyz;
    point0=positionMatrix*scaleMatrix*rotateMatrix*point;
    height = point.y;
}
