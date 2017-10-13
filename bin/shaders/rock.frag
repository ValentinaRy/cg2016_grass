#version 330

in vec2 TexCoord0;
in vec3 normal0;
in vec4 point0;

uniform sampler2D rockTex;
uniform vec3 lightColor;
uniform float ambientIntensity;
uniform vec3 direction;
uniform float diffuseIntensity;
uniform float specularIntensity;
uniform vec3 eyepos;
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
    vec4 SpecularColor = vec4(0, 0, 0, 0);
    if (DiffuseFactor > 0){
        DiffuseColor = vec4(lightColor, 1.0)*diffuseIntensity*DiffuseFactor;
        vec3 VertexToEye = normalize(eyepos - point0.xyz);
        vec3 LightReflect = normalize(reflect(direction, normal0));
        float SpecularFactor = dot(VertexToEye, LightReflect);
        SpecularFactor = pow(SpecularFactor, specularIntensity);
        if (SpecularFactor > 0) {
            SpecularColor = vec4(lightColor, 1.0f)*SpecularFactor;
        }
    }
    vec4 PointColor = vec4(0,0,0,0);
    float PointFactor = dot (normalize(normal0), normalize(-point0.xyz+pointPosition))+1;
    if (PointFactor > 0) {
        float dist = sqrt(sqr(point0.x-pointPosition.x)+sqr(point0.z-pointPosition.z)+sqr(point0.z-pointPosition.z));
        PointColor = vec4(pointColor,1.0)*3/(1+5*dist+3*sqr(dist))*PointFactor*pointIntensity/2;
    }
    outColor = vec4((texture2D(rockTex, TexCoord0)
    *(AmbientColor*0.5+DiffuseColor*0.6+PointColor)*(0.9+SpecularColor)).xyz,0);//vec4(0.3,0.1,0.1,0);
}
