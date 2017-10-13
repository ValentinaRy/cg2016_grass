#version 330

in vec2 groundTexCoord;
in vec3 normal0;
in vec4 point0;

uniform sampler2D groundTex;
uniform vec3 lightColor;
uniform float ambientIntensity;
uniform vec3 direction;
uniform float diffuseIntensity;
uniform vec3 pointColor;
uniform vec3 pointPosition;
uniform float pointIntensity;

out vec4 outColor;

float sqr(float x) {
    return x*x;
}

void main() {
    vec4 AmbientColor = vec4(lightColor, 1.0)*ambientIntensity;
    float DiffuseFactor = dot(normalize(normal0), -direction);
    vec4 DiffuseColor = vec4(0, 0, 0, 0);
    if (DiffuseFactor > 0){
        DiffuseColor = vec4(lightColor, 1.0)*diffuseIntensity*DiffuseFactor;
    }
    float dist = sqrt(sqr(point0.x-pointPosition.x)+sqr(point0.z-pointPosition.z)+sqr(point0.z-pointPosition.z));
    vec4 PointColor = vec4(pointColor,1.0)*3/(1+5*dist+9*sqr(dist))*pointIntensity;
    outColor = vec4((texture2D(groundTex, groundTexCoord)
    *(AmbientColor*0.4+DiffuseColor*0.5+PointColor)).xyz,0)*(point0.y<0?0.3:1);//vec4(0.3, 0.3, 0, 0);
}
