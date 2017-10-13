#include <iostream>
#include <vector>
#include <SOIL.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <ctime>

#include "Utility.h"
#include "serializer.hpp"

using namespace std;

const string binaryfile="binary", textfile="text";

clock_t mytimer;

enum {GRASS, GROUND, ROCK, LAMP, BORDER, LAKE};

struct lighting {
    float daytime;
    VM::vec3 color;
    float ambientIntensity;
    VM::vec3 direction;
    float diffuseIntensity;
    float specularIntensity;
    VM::vec3 pointColor;
    VM::vec3 pointPosition;
    float pointIntensity;
};

struct Mesh {
    vector<VM::vec4> Points;
    vector<uint> Indices;
    vector<VM::vec3> Normals;
    vector<VM::vec2> TexCoords;
};

GL::Camera camera;               // Мы предоставляем Вам реализацию камеры. В OpenGL камера - это просто 2 матрицы. Модельно-видовая матрица и матрица проекции. // ###
                                 // Задача этого класса только в том чтобы обработать ввод с клавиатуры и правильно сформировать эти матрицы.
                                 // Вы можете просто пользоваться этим классом для расчёта указанных матриц.


GLuint grassPointsCount; // Количество вершин у модели травинки
GLuint grassShader;      // Шейдер, рисующий траву
GLuint grassVAO;         // VAO для травы (что такое VAO почитайте в доках)
GLuint grassVariance;    // Буфер для смещения координат травинок
GLuint scaleBuffer;
GLuint alphaBuffer;
GLuint positionBuffer;


GLuint groundShader; // Шейдер для земли
GLuint groundVAO; // VAO для земли
GLuint groundBuffer;
Mesh groundMesh;

GLuint rockPointsCount;
GLuint rockShader;
GLuint rockVAO;
VM::vec3 rockPosition;
float rocksize;

GLuint lampPointsCount;
GLuint lampShader;
GLuint lampVAO;
VM::vec3 lampPosition;
float lampsize;

GLuint borderPointsCount;
GLuint borderShader;
GLuint borderVAO;

GLuint lakePointsCount;
GLuint lakeShader;
GLuint lakeVAO;

// Размеры экрана
uint screenWidth = 800;
uint screenHeight = 600;

const int MapSize = 10;

// Это для захвата мышки. Вам это не потребуется (это не значит, что нужно удалять эту строку)
bool captureMouse = true;

vector<VM::vec3> GenerateGrassPositions();
void genHeightMap();
float myrand(float start=0, float end=1) {
    return float(rand())/RAND_MAX*(end-start)+start;
}

class State {
public:
    vector<VM::vec4> grassVarianceData; // Вектор со смещениями для координат травинок
    vector<VM::vec3> grassPositions;
    vector<VM::vec3> grassScale;
    vector<float> grassAlpha;
    float heightMap[MapSize+1][MapSize+1];
    bool changetime, randwind;
    lighting light;
    int decInstances;
    int GRASS_INSTANCES;
    VM::vec3 Wind;
    State() {
        srand(time(NULL));
        Wind = VM::vec3(0.5, 0, 0);
        rockPosition=VM::vec3(0.2, 0.0, 0.3);
        rocksize=0.1;
        lampPosition=VM::vec3(0.235, 0.04, 0.35);
        lampsize=0.03;
        genHeightMap();
        GRASS_INSTANCES = 20000;
        grassPositions = GenerateGrassPositions();
        for (int i=0; i<GRASS_INSTANCES-decInstances;++i) {
            grassScale.push_back(VM::vec3(myrand(1,2),myrand(1,2),myrand(1,2)));
            grassAlpha.push_back(myrand(0,3.14159265));
        }
        for (uint i = 0; i < GRASS_INSTANCES - decInstances; ++i) {
          grassVarianceData.push_back(VM::vec4(0,0,0,0));
        }
        light.daytime = 0.2;
        light.color=VM::vec3(1,1,1);
        light.ambientIntensity = 0.8*sqr(sin(3.14159265*light.daytime));
        light.direction=VM::vec3(0,-sin(3.14159265*light.daytime),cos(3.14159265*light.daytime));
        light.diffuseIntensity= sin(3.14159265*light.daytime);
        light.specularIntensity=30*(1+sin(3.14159265*light.daytime));
        light.pointColor=VM::vec3(1,0.6,0.2);
        light.pointPosition=lampPosition+VM::vec3(lampsize,lampsize,lampsize)/2;
        light.pointIntensity=1;

        changetime = true;
        randwind = true;
    }
    bool Serialize(Serializer &inout) {
        bool res = true;
        res &= inout.InOut(decInstances);
        res &= inout.InOut(GRASS_INSTANCES);
        int ms = MapSize;
        res &= inout.InOut(ms);
        if (ms != MapSize) {
            cout << "MapSize isn't match\n";
            return false;
        }
        uint N = GRASS_INSTANCES-decInstances;
        for (uint i=0; i<N;++i) {
            res &= inout.InOut(grassVarianceData[i]);
        }
        res &= inout.InOut(Wind);
        for (uint i=0; i<N;++i) {
            res &= inout.InOut(grassPositions[i]);
        }
        for (uint i=0; i<N;++i) {
            res &= inout.InOut(grassScale[i]);
        }
        for (uint i=0; i<N;++i) {
            res &= inout.InOut(grassAlpha[i]);
        }
        for (uint i=0; i<MapSize+1;++i) {
            for (uint j=0; j<MapSize+1;++j) {
                res &= inout.InOut(heightMap[i][j]);
            }
        }
        res &= inout.InOut(changetime);
        res &= inout.InOut(randwind);
        res &= inout.InOut(light.daytime);
        res &= inout.InOut(light.color);
        res &= inout.InOut(light.ambientIntensity);
        res &= inout.InOut(light.direction);
        res &= inout.InOut(light.diffuseIntensity);
        res &= inout.InOut(light.specularIntensity);
        res &= inout.InOut(light.pointColor);
        res &= inout.InOut(light.pointPosition);
        res &= inout.InOut(light.pointIntensity);
        return res;
    }
} S;

class Texture
{
public:
    Texture(GLenum TextureTarget, const std::string& FileName) {
        m_textureTarget = TextureTarget;
        m_fileName      = FileName;
    }

    bool Load() {
        glGenTextures(1, &m_textureObj);
        glBindTexture(m_textureTarget, m_textureObj);

        glTexParameterf(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    	glTexParameterf(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


        image = SOIL_load_image(m_fileName.c_str(), &width, &height, 0, SOIL_LOAD_RGBA);

        glTexImage2D(m_textureTarget, 0, GL_RGBA, width, height, -0.5, GL_RGBA, GL_UNSIGNED_BYTE, image);
        glGenerateMipmap(m_textureTarget);

        SOIL_free_image_data(image);
        glBindTexture(m_textureTarget, 0);
        return true;
    }

    void Bind(GLuint program, const string& name, GLint unit) {
        glActiveTexture(GL_TEXTURE0 + unit);                                         CHECK_GL_ERRORS;
        glBindTexture(m_textureTarget, m_textureObj);                                       CHECK_GL_ERRORS;

        GLint location = glGetUniformLocation(program, name.c_str());                CHECK_GL_ERRORS;
        if(location >=0 ) {
            glUniform1i(location, unit);                                             CHECK_GL_ERRORS;
        }
    }

private:
    std::string m_fileName;
    GLenum m_textureTarget;
    GLuint m_textureObj;
    unsigned char *image;
    int width, height;
};

Texture *pGrassTexture, *pGroundTexture, *pRockTexture;


void genHeightMap() {
    //(x-0.7)^2+(z-0.6)^2<=0.2^2
    float step = 1.0/MapSize;
    for (int i=0; i<MapSize; ++i) {
        for (int j=0; j<MapSize; ++j) {
            VM::vec3 delta = VM::vec3(i*step,0,j*step) - rockPosition;
            if (delta.x<-step || delta.x>rocksize+step || delta.z<-step || delta.z>rocksize+step) {
                if (sqr(i*step-0.7)+sqr(j*step-0.6)<=sqr(0.2)) {
                    S.heightMap[i][j] = -myrand(0,0.1);
                } else {
                    S.heightMap[i][j] = 0.01+myrand(0,0.1);
                }
            } else {
                S.heightMap[i][j] = 0.01;
            }
        }
    }
}

// Генерация позиций травинок (эту функцию вам придётся переписать)
vector<VM::vec3> GenerateGrassPositions() {
    vector<VM::vec3> grassPositions;
    uint cnt = round(sqrt(S.GRASS_INSTANCES));
    for (uint i = 0; i < S.GRASS_INSTANCES; ++i) {
        VM::vec3 pos = VM::vec3((i % cnt) / float(cnt), 0.0, (i / cnt) / float(cnt))
                        + VM::vec3(1, 0, 1)/float(cnt*2)
                        + VM::vec3((myrand(-1,1))/(cnt*4), 0, (myrand(-1,1))/(cnt*4));
        VM::vec3 delta = pos - rockPosition;
        if (delta.x<0 || delta.x>rocksize || delta.z<0 || delta.z>rocksize) {
            int i1 = floor(pos.x*MapSize);
            int j1 = floor(pos.z*MapSize);
            float step = 1.0/MapSize;
            float x0=(pos.x/step-i1);
            float z0=(pos.z/step-j1);
            float y1=S.heightMap[i1][j1];
            float y2=S.heightMap[i1+1][j1];
            float y3=S.heightMap[i1][j1+1];
            float y4=S.heightMap[i1+1][j1+1];
            float y;
            if (1-x0>=z0) {
                y=y1+(y2-y1)*x0+(y3-y1)*z0;
            } else {
                y=y4+(y2-y4)*(1-z0)+(y3-y4)*(1-x0);
            }
            if (y > 0.001) {
                pos += VM::vec3(0,y,0);
                grassPositions.push_back(pos);
            } else {
                S.decInstances++;
            }
        } else {
            S.decInstances++;
        }
    }
    return grassPositions;
}

// Функция, рисующая замлю
void DrawGround() {
    // Используем шейдер для земли
    glUseProgram(groundShader);                                                  CHECK_GL_ERRORS

    // Устанавливаем юниформ для шейдера. В данном случае передадим перспективную матрицу камеры
    // Находим локацию юниформа 'camera' в шейдере
    GLint cameraLocation = glGetUniformLocation(groundShader, "camera");         CHECK_GL_ERRORS
    // Устанавливаем юниформ (загружаем на GPU матрицу проекции?)                                                     // ###
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS

    GLint colorlocation = glGetUniformLocation(groundShader, "lightColor");
    glUniform3fv(colorlocation, 1, &S.light.color.x);
    GLint intensitylocation = glGetUniformLocation(groundShader, "ambientIntensity");    CHECK_GL_ERRORS
    glUniform1fv(intensitylocation, 1, &S.light.ambientIntensity);                        CHECK_GL_ERRORS
    float dirnorm = sqrt(sqr(S.light.direction.x)+sqr(S.light.direction.y)+sqr(S.light.direction.z)); CHECK_GL_ERRORS
    if (dirnorm != 0) {
        S.light.direction.x /= dirnorm;
        S.light.direction.y /= dirnorm;
        S.light.direction.z /= dirnorm;
    }
    GLint directionlocation = glGetUniformLocation(groundShader, "direction");               CHECK_GL_ERRORS
    glUniform3fv(directionlocation, 1, &S.light.direction.x);                                 CHECK_GL_ERRORS
    GLint diffuseIntensitylocation = glGetUniformLocation(groundShader, "diffuseIntensity"); CHECK_GL_ERRORS
    glUniform1fv(diffuseIntensitylocation, 1, &S.light.diffuseIntensity);                     CHECK_GL_ERRORS
    GLint eyeposlocation = glGetUniformLocation(groundShader, "eyepos");    CHECK_GL_ERRORS
    glUniform3fv(eyeposlocation, 1, &camera.position.x);                        CHECK_GL_ERRORS
    GLint specularlocation = glGetUniformLocation(groundShader, "specularIntensity");    CHECK_GL_ERRORS
    glUniform1fv(specularlocation, 1, &S.light.specularIntensity);                        CHECK_GL_ERRORS
    GLint pointPositionlocation = glGetUniformLocation(groundShader, "pointPosition");    CHECK_GL_ERRORS
    glUniform3fv(pointPositionlocation, 1, &S.light.pointPosition.x);                        CHECK_GL_ERRORS
    GLint pointColorlocation = glGetUniformLocation(groundShader, "pointColor");    CHECK_GL_ERRORS
    glUniform3fv(pointColorlocation, 1, &S.light.pointColor.x);                        CHECK_GL_ERRORS
    GLint pointIntensitylocation = glGetUniformLocation(groundShader, "pointIntensity");    CHECK_GL_ERRORS
    glUniform1fv(pointIntensitylocation, 1, &S.light.pointIntensity);                        CHECK_GL_ERRORS

    // Подключаем VAO, который содержит буферы, необходимые для отрисовки земли
    glBindVertexArray(groundVAO);                                                CHECK_GL_ERRORS

    pGroundTexture->Bind(groundShader, "groundTex", 0);

    // Рисуем землю: 2 треугольника (6 вершин)
    //glDrawArrays(GL_TRIANGLES, 0, 200);                                           CHECK_GL_ERRORS
    glDrawElementsBaseVertex(GL_TRIANGLES,6*MapSize*MapSize,GL_UNSIGNED_INT,0,0);

    // Отсоединяем VAO
    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    // Отключаем шейдер
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

// Обновление смещения травинок
void UpdateGrassVariance() {
    static bool isdec = true;
    if (S.randwind&&myrand() < 0.5) {
        float dx = myrand(0,0.01);
        if (myrand() < 0.15) isdec = !isdec;
        if ((S.Wind.x > 0.8 || isdec)&&S.Wind.x-dx>-0.8) dx = -dx;
        S.Wind.x += dx;
    }
    // Генерация случайных смещений
    for (int i=0; i < S.GRASS_INSTANCES; ++i) {
      //S.grassVarianceData[i].x = (float)rand() / RAND_MAX / 100;
      //S.grassVarianceData[i].y = (float)rand() / RAND_MAX / 10;
      //S.grassVarianceData[i].z = (float)rand() / RAND_MAX / 100;
      S.grassVarianceData[i] = VM::vec4(S.Wind,0);
    }
    // Привязываем буфер, содержащий смещения
    glBindBuffer(GL_ARRAY_BUFFER, grassVariance);                                CHECK_GL_ERRORS
    // Загружаем данные в видеопамять
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * S.grassVarianceData.size(), S.grassVarianceData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS
    // Отвязываем буфер

    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}


// Рисование травы
void DrawGrass() {
    // Тут то же самое, что и в рисовании земли
    glUseProgram(grassShader);                                                   CHECK_GL_ERRORS
    GLint cameraLocation = glGetUniformLocation(grassShader, "camera");          CHECK_GL_ERRORS
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS
    GLint colorlocation = glGetUniformLocation(grassShader, "lightColor");
    glUniform3fv(colorlocation, 1, &S.light.color.x);
    GLint intensitylocation = glGetUniformLocation(grassShader, "ambientIntensity");    CHECK_GL_ERRORS
    glUniform1fv(intensitylocation, 1, &S.light.ambientIntensity);                        CHECK_GL_ERRORS
    float dirnorm = sqrt(sqr(S.light.direction.x)+sqr(S.light.direction.y)+sqr(S.light.direction.z)); CHECK_GL_ERRORS
    if (dirnorm != 0) {
        S.light.direction.x /= dirnorm;
        S.light.direction.y /= dirnorm;
        S.light.direction.z /= dirnorm;
    }
    GLint directionlocation = glGetUniformLocation(grassShader, "direction");               CHECK_GL_ERRORS
    glUniform3fv(directionlocation, 1, &S.light.direction.x);                                 CHECK_GL_ERRORS
    GLint diffuseIntensitylocation = glGetUniformLocation(grassShader, "diffuseIntensity"); CHECK_GL_ERRORS
    glUniform1fv(diffuseIntensitylocation, 1, &S.light.diffuseIntensity);                     CHECK_GL_ERRORS
    GLint eyeposlocation = glGetUniformLocation(grassShader, "eyepos");    CHECK_GL_ERRORS
    glUniform3fv(eyeposlocation, 1, &camera.position.x);                        CHECK_GL_ERRORS
    GLint specularlocation = glGetUniformLocation(grassShader, "specularIntensity");    CHECK_GL_ERRORS
    glUniform1fv(specularlocation, 1, &S.light.specularIntensity);                        CHECK_GL_ERRORS
    GLint pointPositionlocation = glGetUniformLocation(grassShader, "pointPosition");    CHECK_GL_ERRORS
    glUniform3fv(pointPositionlocation, 1, &S.light.pointPosition.x);                        CHECK_GL_ERRORS
    GLint pointColorlocation = glGetUniformLocation(grassShader, "pointColor");    CHECK_GL_ERRORS
    glUniform3fv(pointColorlocation, 1, &S.light.pointColor.x);                        CHECK_GL_ERRORS
    GLint pointIntensitylocation = glGetUniformLocation(grassShader, "pointIntensity");    CHECK_GL_ERRORS
    glUniform1fv(pointIntensitylocation, 1, &S.light.pointIntensity);                        CHECK_GL_ERRORS


    glBindVertexArray(grassVAO);                                                 CHECK_GL_ERRORS
    // Обновляем смещения для травы
    UpdateGrassVariance();
    pGrassTexture->Bind(grassShader, "grassTex", 0);
    // Отрисовка травинок в количестве GRASS_INSTANCES
    //glDrawArraysInstanced(GL_TRIANGLES, 0, grassPointsCount, GRASS_INSTANCES);   CHECK_GL_ERRORS
    //glDrawElementsBaseVertex(GL_TRIANGLES,grassPointsCount,GL_UNSIGNED_INT,0,0);
    glDrawElementsInstanced(GL_TRIANGLES,grassPointsCount,GL_UNSIGNED_INT,0,S.GRASS_INSTANCES - S.decInstances);
    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

void DrawRock() {
    glUseProgram(rockShader);                                                  CHECK_GL_ERRORS
    GLint cameraLocation = glGetUniformLocation(rockShader, "camera");         CHECK_GL_ERRORS
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS
    GLint positionLocation = glGetUniformLocation(rockShader, "position");         CHECK_GL_ERRORS
    glUniform3fv(positionLocation, 1, &rockPosition.x); CHECK_GL_ERRORS
    GLint scaleLocation = glGetUniformLocation(rockShader, "scale");         CHECK_GL_ERRORS
    glUniform1fv(scaleLocation, 1, &rocksize); CHECK_GL_ERRORS
    GLint colorlocation = glGetUniformLocation(rockShader, "lightColor");
    glUniform3fv(colorlocation, 1, &S.light.color.x);
    GLint intensitylocation = glGetUniformLocation(rockShader, "ambientIntensity");    CHECK_GL_ERRORS
    glUniform1fv(intensitylocation, 1, &S.light.ambientIntensity);                        CHECK_GL_ERRORS
    float dirnorm = sqrt(sqr(S.light.direction.x)+sqr(S.light.direction.y)+sqr(S.light.direction.z)); CHECK_GL_ERRORS
    if (dirnorm != 0) {
        S.light.direction.x /= dirnorm;
        S.light.direction.y /= dirnorm;
        S.light.direction.z /= dirnorm;
    }
    GLint directionlocation = glGetUniformLocation(rockShader, "direction");               CHECK_GL_ERRORS
    glUniform3fv(directionlocation, 1, &S.light.direction.x);                                 CHECK_GL_ERRORS
    GLint diffuseIntensitylocation = glGetUniformLocation(rockShader, "diffuseIntensity"); CHECK_GL_ERRORS
    glUniform1fv(diffuseIntensitylocation, 1, &S.light.diffuseIntensity);                     CHECK_GL_ERRORS
    GLint eyeposlocation = glGetUniformLocation(rockShader, "eyepos");    CHECK_GL_ERRORS
    glUniform3fv(eyeposlocation, 1, &camera.position.x);                        CHECK_GL_ERRORS
    GLint specularlocation = glGetUniformLocation(rockShader, "specularIntensity");    CHECK_GL_ERRORS
    glUniform1fv(specularlocation, 1, &S.light.specularIntensity);                        CHECK_GL_ERRORS
    GLint pointPositionlocation = glGetUniformLocation(rockShader, "pointPosition");    CHECK_GL_ERRORS
    glUniform3fv(pointPositionlocation, 1, &S.light.pointPosition.x);                        CHECK_GL_ERRORS
    GLint pointColorlocation = glGetUniformLocation(rockShader, "pointColor");    CHECK_GL_ERRORS
    glUniform3fv(pointColorlocation, 1, &S.light.pointColor.x);                        CHECK_GL_ERRORS
    GLint pointIntensitylocation = glGetUniformLocation(rockShader, "pointIntensity");    CHECK_GL_ERRORS
    glUniform1fv(pointIntensitylocation, 1, &S.light.pointIntensity);                        CHECK_GL_ERRORS

    glBindVertexArray(rockVAO);                                                CHECK_GL_ERRORS

    pRockTexture->Bind(rockShader, "rockTex", 0);

    glDrawElementsBaseVertex(GL_TRIANGLES,rockPointsCount,GL_UNSIGNED_INT,0,0);

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

void DrawLamp() {
    glUseProgram(lampShader);                                                  CHECK_GL_ERRORS
    GLint cameraLocation = glGetUniformLocation(lampShader, "camera");         CHECK_GL_ERRORS
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS
    GLint positionLocation = glGetUniformLocation(lampShader, "position");         CHECK_GL_ERRORS
    glUniform3fv(positionLocation, 1, &lampPosition.x); CHECK_GL_ERRORS
    GLint scaleLocation = glGetUniformLocation(lampShader, "scale");         CHECK_GL_ERRORS
    glUniform1fv(scaleLocation, 1, &lampsize); CHECK_GL_ERRORS
    GLint colorlocation = glGetUniformLocation(lampShader, "lightColor");
    glUniform3fv(colorlocation, 1, &S.light.color.x);
    GLint intensitylocation = glGetUniformLocation(lampShader, "ambientIntensity");    CHECK_GL_ERRORS
    glUniform1fv(intensitylocation, 1, &S.light.ambientIntensity);                        CHECK_GL_ERRORS
    float dirnorm = sqrt(sqr(S.light.direction.x)+sqr(S.light.direction.y)+sqr(S.light.direction.z)); CHECK_GL_ERRORS
    if (dirnorm != 0) {
        S.light.direction.x /= dirnorm;
        S.light.direction.y /= dirnorm;
        S.light.direction.z /= dirnorm;
    }
    GLint directionlocation = glGetUniformLocation(lampShader, "direction");               CHECK_GL_ERRORS
    glUniform3fv(directionlocation, 1, &S.light.direction.x);                                 CHECK_GL_ERRORS
    GLint diffuseIntensitylocation = glGetUniformLocation(lampShader, "diffuseIntensity"); CHECK_GL_ERRORS
    glUniform1fv(diffuseIntensitylocation, 1, &S.light.diffuseIntensity);                     CHECK_GL_ERRORS
    GLint eyeposlocation = glGetUniformLocation(lampShader, "eyepos");    CHECK_GL_ERRORS
    glUniform3fv(eyeposlocation, 1, &camera.position.x);                        CHECK_GL_ERRORS
    GLint specularlocation = glGetUniformLocation(lampShader, "specularIntensity");    CHECK_GL_ERRORS
    glUniform1fv(specularlocation, 1, &S.light.specularIntensity);                        CHECK_GL_ERRORS
    GLint pointPositionlocation = glGetUniformLocation(lampShader, "pointPosition");    CHECK_GL_ERRORS
    glUniform3fv(pointPositionlocation, 1, &S.light.pointPosition.x);                        CHECK_GL_ERRORS
    GLint pointColorlocation = glGetUniformLocation(lampShader, "pointColor");    CHECK_GL_ERRORS
    glUniform3fv(pointColorlocation, 1, &S.light.pointColor.x);                        CHECK_GL_ERRORS
    GLint pointIntensitylocation = glGetUniformLocation(lampShader, "pointIntensity");    CHECK_GL_ERRORS
    glUniform1fv(pointIntensitylocation, 1, &S.light.pointIntensity);                        CHECK_GL_ERRORS

    glBindVertexArray(lampVAO);                                                CHECK_GL_ERRORS

    glDrawElementsBaseVertex(GL_TRIANGLES,lampPointsCount,GL_UNSIGNED_INT,0,0);

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

void DrawBorder() {
    glUseProgram(borderShader);                                                  CHECK_GL_ERRORS
    GLint cameraLocation = glGetUniformLocation(borderShader, "camera");         CHECK_GL_ERRORS
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS
    GLint positionLocation = glGetUniformLocation(borderShader, "position");         CHECK_GL_ERRORS
    glUniform3fv(positionLocation, 1, &lampPosition.x); CHECK_GL_ERRORS
    GLint scaleLocation = glGetUniformLocation(borderShader, "scale");         CHECK_GL_ERRORS
    glUniform1fv(scaleLocation, 1, &lampsize); CHECK_GL_ERRORS
    GLint colorlocation = glGetUniformLocation(borderShader, "lightColor");
    glUniform3fv(colorlocation, 1, &S.light.color.x);
    GLint intensitylocation = glGetUniformLocation(borderShader, "ambientIntensity");    CHECK_GL_ERRORS
    glUniform1fv(intensitylocation, 1, &S.light.ambientIntensity);                        CHECK_GL_ERRORS
    float dirnorm = sqrt(sqr(S.light.direction.x)+sqr(S.light.direction.y)+sqr(S.light.direction.z)); CHECK_GL_ERRORS
    if (dirnorm != 0) {
        S.light.direction.x /= dirnorm;
        S.light.direction.y /= dirnorm;
        S.light.direction.z /= dirnorm;
    }
    GLint directionlocation = glGetUniformLocation(borderShader, "direction");               CHECK_GL_ERRORS
    glUniform3fv(directionlocation, 1, &S.light.direction.x);                                 CHECK_GL_ERRORS
    GLint diffuseIntensitylocation = glGetUniformLocation(borderShader, "diffuseIntensity"); CHECK_GL_ERRORS
    glUniform1fv(diffuseIntensitylocation, 1, &S.light.diffuseIntensity);                     CHECK_GL_ERRORS
    GLint eyeposlocation = glGetUniformLocation(borderShader, "eyepos");    CHECK_GL_ERRORS
    glUniform3fv(eyeposlocation, 1, &camera.position.x);                        CHECK_GL_ERRORS
    GLint specularlocation = glGetUniformLocation(borderShader, "specularIntensity");    CHECK_GL_ERRORS
    glUniform1fv(specularlocation, 1, &S.light.specularIntensity);                        CHECK_GL_ERRORS
    GLint pointPositionlocation = glGetUniformLocation(borderShader, "pointPosition");    CHECK_GL_ERRORS
    glUniform3fv(pointPositionlocation, 1, &S.light.pointPosition.x);                        CHECK_GL_ERRORS
    GLint pointColorlocation = glGetUniformLocation(borderShader, "pointColor");    CHECK_GL_ERRORS
    glUniform3fv(pointColorlocation, 1, &S.light.pointColor.x);                        CHECK_GL_ERRORS
    GLint pointIntensitylocation = glGetUniformLocation(borderShader, "pointIntensity");    CHECK_GL_ERRORS
    glUniform1fv(pointIntensitylocation, 1, &S.light.pointIntensity);                        CHECK_GL_ERRORS

    glBindVertexArray(borderVAO);                                                CHECK_GL_ERRORS

    glDrawElementsBaseVertex(GL_TRIANGLES,borderPointsCount,GL_UNSIGNED_INT,0,0);

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

void DrawLake() {
    glUseProgram(lakeShader);                                                  CHECK_GL_ERRORS
    GLint cameraLocation = glGetUniformLocation(lakeShader, "camera");         CHECK_GL_ERRORS
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS
    GLint colorlocation = glGetUniformLocation(lakeShader, "lightColor");
    glUniform3fv(colorlocation, 1, &S.light.color.x);
    GLint intensitylocation = glGetUniformLocation(lakeShader, "ambientIntensity");    CHECK_GL_ERRORS
    glUniform1fv(intensitylocation, 1, &S.light.ambientIntensity);                        CHECK_GL_ERRORS
    float dirnorm = sqrt(sqr(S.light.direction.x)+sqr(S.light.direction.y)+sqr(S.light.direction.z)); CHECK_GL_ERRORS
    if (dirnorm != 0) {
        S.light.direction.x /= dirnorm;
        S.light.direction.y /= dirnorm;
        S.light.direction.z /= dirnorm;
    }
    GLint directionlocation = glGetUniformLocation(lakeShader, "direction");               CHECK_GL_ERRORS
    glUniform3fv(directionlocation, 1, &S.light.direction.x);                                 CHECK_GL_ERRORS
    GLint diffuseIntensitylocation = glGetUniformLocation(lakeShader, "diffuseIntensity"); CHECK_GL_ERRORS
    glUniform1fv(diffuseIntensitylocation, 1, &S.light.diffuseIntensity);                     CHECK_GL_ERRORS
    GLint eyeposlocation = glGetUniformLocation(lakeShader, "eyepos");    CHECK_GL_ERRORS
    glUniform3fv(eyeposlocation, 1, &camera.position.x);                        CHECK_GL_ERRORS
    GLint specularlocation = glGetUniformLocation(lakeShader, "specularIntensity");    CHECK_GL_ERRORS
    glUniform1fv(specularlocation, 1, &S.light.specularIntensity);                        CHECK_GL_ERRORS
    GLint pointPositionlocation = glGetUniformLocation(lakeShader, "pointPosition");    CHECK_GL_ERRORS
    glUniform3fv(pointPositionlocation, 1, &S.light.pointPosition.x);                        CHECK_GL_ERRORS
    GLint pointColorlocation = glGetUniformLocation(lakeShader, "pointColor");    CHECK_GL_ERRORS
    glUniform3fv(pointColorlocation, 1, &S.light.pointColor.x);                        CHECK_GL_ERRORS
    GLint pointIntensitylocation = glGetUniformLocation(lakeShader, "pointIntensity");    CHECK_GL_ERRORS
    glUniform1fv(pointIntensitylocation, 1, &S.light.pointIntensity);                        CHECK_GL_ERRORS

    glBindVertexArray(lakeVAO);                                                CHECK_GL_ERRORS

    glDrawElementsBaseVertex(GL_TRIANGLES,lakePointsCount,GL_UNSIGNED_INT,0,0);

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

void updateDayTime() {
    if (S.light.daytime>1) S.light.daytime -= 2;
    if (S.light.daytime<-1) S.light.daytime += 2;
    if (S.light.daytime >0) {
        S.light.color=VM::vec3(1,1,0.9);
        S.light.ambientIntensity = 0.8*sqr(sin(3.14159265*S.light.daytime));
        S.light.direction=VM::vec3(0,-sin(3.14159265*S.light.daytime),cos(3.14159265*S.light.daytime));
        S.light.diffuseIntensity= sin(3.14159265*S.light.daytime);
        S.light.specularIntensity=30;//*(1+sin(3.14159265*S.light.daytime));
    } else {
        S.light.color=VM::vec3(0.6,0.8,1);
        S.light.direction=VM::vec3(0,sin(3.14159265*S.light.daytime),-cos(3.14159265*S.light.daytime));
        S.light.ambientIntensity = 0.4*sqr(sin(3.14159265*S.light.daytime));
        S.light.diffuseIntensity= -0.5*sin(3.14159265*S.light.daytime);
        S.light.specularIntensity=30;//*(1-sin(3.14159265*S.light.daytime));
    }
}

// Эта функция вызывается для обновления экрана
void RenderLayouts() {
    if (float(clock() - mytimer)/CLOCKS_PER_SEC > 0.016) {
        if (S.changetime) S.light.daytime += 0.005;
        updateDayTime();
        float delt = sin(3.14159265*S.light.daytime)>0?sin(3.14159265*S.light.daytime):sin(3.14159265*0.005);
        VM::vec3 skycolor = VM::vec3(0.659, 1, 1.0)*delt;
        glClearColor(skycolor.x,skycolor.y,skycolor.z, 0.0); //A8FFFF
        // Включение буфера глубины
        glEnable(GL_DEPTH_TEST);
        // Очистка буфера глубины и цветового буфера
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Рисуем меши
        glEnable(GL_BLEND);
        glEnable(GL_MULTISAMPLE);
        glBlendFunc(GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA);
        DrawGround();
        DrawGrass();
        DrawRock();
        DrawLamp();
        DrawBorder();
        DrawLake();
        glDisable(GL_BLEND);
        glutSwapBuffers();
        mytimer = clock();
    }
}

// Завершение программы
void FinishProgram() {
    glutDestroyWindow(glutGetWindow());
}

void GenMesh(Mesh &mesh, int type);

// Обработка события нажатия клавиши (специальные клавиши обрабатываются в функции SpecialButtons)
void KeyboardEvents(unsigned char key, int x, int y) {
    if (key == 27) {
        FinishProgram();
    } else if (key == 'w') {
        camera.goForward();
    } else if (key == 's') {
        camera.goBack();
    } else if (key == 'm') {
        captureMouse = !captureMouse;
        if (captureMouse) {
            glutWarpPointer(screenWidth / 2, screenHeight / 2);
            glutSetCursor(GLUT_CURSOR_NONE);
        } else {
            glutSetCursor(GLUT_CURSOR_RIGHT_ARROW);
        }
    } else if (key == '-') {
        if (S.Wind.x+0.1 < 0.8) S.Wind += VM::vec3(0.1, 0.0, 0.0);
    } else if (key == '=') {
        if (S.Wind.x-0.1 >-0.8) S.Wind -= VM::vec3(0.1, 0.0, 0.0);
    } else if (key=='z'||key=='x') {
        if (key=='z') {
            S.light.daytime += 0.1;
        }
        if (key=='x') {
            S.light.daytime -= 0.1;
        }
        updateDayTime();
    } else if (key=='c') {
        S.light.pointIntensity += 0.1;
        if (S.light.pointIntensity > 1) S.light.pointIntensity = 1;
    } else if (key=='v') {
        S.light.pointIntensity -= 0.1;
        if (S.light.pointIntensity < 0) S.light.pointIntensity = 0;
    } else if (key=='n') {
        S.changetime = !S.changetime;
    } else if (key=='b') {
        S.randwind = !S.randwind;
    } else if (key=='i') {
        BinarySerializerWriter binw(binaryfile);
        if (!binw.Open()) {
            cout << "Can't open to write\n";
            return;
        }
        if (!S.Serialize(binw)) {
            cout <"Can't Serialize to write\n";
            return;
        }
        if (!binw.Close()) {
            cout << "Can't close write\n";
            return;
        }
        cout << "All settings have been saved\n";
    } else if (key=='o') {
        BinarySerializerReader binr(binaryfile);
        if (!binr.Open()) {
            cout << "Can't open to read\n";
            return;
        }
        if (!S.Serialize(binr)) {
            cout << "Can't Serialize read\n";
            return;
        }
        if (!binr.Close()) {
            cout << "Can't close read\n";
            return;
        }
        cout << "All settings have been loaded\n";
        glBindBuffer(GL_ARRAY_BUFFER, scaleBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3) * S.grassScale.size(), S.grassScale.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, alphaBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * S.grassAlpha.size(), S.grassAlpha.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);                               CHECK_GL_ERRORS
        glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3) * S.grassPositions.size(), S.grassPositions.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, grassVariance);                                CHECK_GL_ERRORS
        glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * S.grassVarianceData.size(), S.grassVarianceData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        GenMesh(groundMesh, GROUND);
        glBindBuffer(GL_ARRAY_BUFFER, groundBuffer);
        glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*groundMesh.Points.size(), groundMesh.Points.data(),GL_STATIC_DRAW);CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    } else if (key=='k') {
        TxtSerializerWriter txtw(textfile);
        if (!txtw.Open()) {
            cout << "Can't open to write\n";
            return;
        }
        if (!S.Serialize(txtw)) {
            cout <<"Can't Serialize to write\n";
            return;
        }
        if (!txtw.Close()) {
            cout << "Can't close write\n";
            return;
        }
        cout << "All settings have been saved\n";
    } else if (key=='l') {
        TxtSerializerReader txtr(textfile);
        if (!txtr.Open()) {
            cout << "Can't open to read\n";
            return;
        }
        if (!S.Serialize(txtr)) {
            cout << "Can't Serialize read\n";
            return;
        }
        if (!txtr.Close()) {
            cout << "Can't close read\n";
            return;
        }
        cout << "All settings have been loaded\n";
        glBindBuffer(GL_ARRAY_BUFFER, scaleBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3) * S.grassScale.size(), S.grassScale.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, alphaBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * S.grassAlpha.size(), S.grassAlpha.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);                               CHECK_GL_ERRORS
        glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3) * S.grassPositions.size(), S.grassPositions.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, grassVariance);                                CHECK_GL_ERRORS
        glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * S.grassVarianceData.size(), S.grassVarianceData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

        GenMesh(groundMesh, GROUND);
        glBindBuffer(GL_ARRAY_BUFFER, groundBuffer);
        glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*groundMesh.Points.size(), groundMesh.Points.data(),GL_STATIC_DRAW);CHECK_GL_ERRORS

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

// Обработка события нажатия специальных клавиш
void SpecialButtons(int key, int x, int y) {
    if (key == GLUT_KEY_RIGHT) {
        camera.rotateY(0.02);
    } else if (key == GLUT_KEY_LEFT) {
        camera.rotateY(-0.02);
    } else if (key == GLUT_KEY_UP) {
        camera.rotateTop(-0.02);
    } else if (key == GLUT_KEY_DOWN) {
        camera.rotateTop(0.02);
    }
}

void IdleFunc() {
    glutPostRedisplay();
}

// Обработка события движения мыши
void MouseMove(int x, int y) {
    if (captureMouse) {
        int centerX = screenWidth / 2,
            centerY = screenHeight / 2;
        if (x != centerX || y != centerY) {
            camera.rotateY((x - centerX) / 1000.0f);
            camera.rotateTop((y - centerY) / 1000.0f);
            glutWarpPointer(centerX, centerY);
        }
    }
}

// Обработка нажатия кнопки мыши
void MouseClick(int button, int state, int x, int y) {
}

// Событие изменение размера окна
void windowReshapeFunc(GLint newWidth, GLint newHeight) {
    glViewport(0, 0, newWidth, newHeight);
    screenWidth = newWidth;
    screenHeight = newHeight;

    camera.screenRatio = (float)screenWidth / screenHeight;
}

// Инициализация окна
void InitializeGLUT(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitContextVersion(3, 0);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitWindowPosition(-1, -1);
    glutInitWindowSize(screenWidth, screenHeight);
    glutCreateWindow("Computer Graphics 3");
    glutWarpPointer(400, 300);
    glutSetCursor(GLUT_CURSOR_NONE);

    glutDisplayFunc(RenderLayouts);
    glutKeyboardFunc(KeyboardEvents);
    glutSpecialFunc(SpecialButtons);
    glutIdleFunc(IdleFunc);
    glutPassiveMotionFunc(MouseMove);
    glutMouseFunc(MouseClick);
    glutReshapeFunc(windowReshapeFunc);
}

void loadObject(Mesh &mesh, Texture **pTexture, string dirpath, string objname, int i1, int i2) {
    aiVector3D Zero3D(0, 0, 0);
    Assimp::Importer Importer;
    const aiScene *pScene = Importer.ReadFile((dirpath+objname).c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

    aiMesh *paiMesh = pScene->mMeshes[i1];
    for (uint j=0; j<paiMesh->mNumVertices; ++j) {
        aiVector3D* pPos      = &(paiMesh->mVertices[j]);
        aiVector3D* pNormal   = &(paiMesh->mNormals[j]);
        aiVector3D* pTexCoord = paiMesh->HasTextureCoords(0) ? &(paiMesh->mTextureCoords[0][j]) : &Zero3D;
        mesh.Points.push_back(VM::vec4(pPos->x, pPos->y, pPos->z, 1));
        mesh.Normals.push_back(VM::vec3(pNormal->x, pNormal->y, pNormal->z));
        mesh.TexCoords.push_back(VM::vec2(pTexCoord->x, pTexCoord->y));
    }
    for (uint j=0; j<paiMesh->mNumFaces; ++j) {
        const aiFace& Face = paiMesh->mFaces[j];
        if (Face.mNumIndices == 3) {
            mesh.Indices.push_back(Face.mIndices[0]);
            mesh.Indices.push_back(Face.mIndices[1]);
            mesh.Indices.push_back(Face.mIndices[2]);
        }
    }
    *pTexture = NULL;
    aiMaterial* pMaterial = pScene->mMaterials[i2];
    if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString Path;
        if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
            std::string FullPath = dirpath + Path.data;
            *pTexture = new Texture(GL_TEXTURE_2D, FullPath);
            (*pTexture)->Load();
        }
    }
    if (!*pTexture) {
        *pTexture = new Texture(GL_TEXTURE_2D, "Texture/white.jpg");
        (*pTexture)->Load();
    }
}

// Здесь вам нужно будет генерировать меш
void GenMesh(Mesh &mesh, int type) {
    mesh.Points.clear();
    mesh.Indices.clear();
    mesh.Normals.clear();
    switch (type) {
        case GRASS: {
            mesh.Points.push_back(VM::vec4(0.5, 0, 0, 1));
            mesh.Points.push_back(VM::vec4(0, 0, 0.1, 1));
            mesh.Points.push_back(VM::vec4(0.5, 0, 0.2, 1));
            mesh.Points.push_back(VM::vec4(1, 0, 0.1, 1));
            mesh.Points.push_back(VM::vec4(0.5, 0.25, 0, 1));
            mesh.Points.push_back(VM::vec4(0, 0.25, 0.1, 1));
            mesh.Points.push_back(VM::vec4(0.5, 0.25, 0.2, 1));
            mesh.Points.push_back(VM::vec4(1, 0.25, 0.1, 1));
            mesh.Points.push_back(VM::vec4(0.5, 0.5, 0.25, 1));
            mesh.Points.push_back(VM::vec4(0, 0.5, 0.35, 1));
            mesh.Points.push_back(VM::vec4(0.5, 0.5, 0.45, 1));
            mesh.Points.push_back(VM::vec4(1, 0.5, 0.35, 1));
            mesh.Points.push_back(VM::vec4(0.5, 0.75, 0.75, 1));
            mesh.Points.push_back(VM::vec4(0, 0.75, 0.85, 1));
            mesh.Points.push_back(VM::vec4(0.5, 0.75, 0.95, 1));
            mesh.Points.push_back(VM::vec4(1, 0.75, 0.85, 1));
            mesh.Points.push_back(VM::vec4(0.5, 1, 1, 1));
            uint arr[] = {
                12,13,16,
                13,14,16,
                14,15,16,
                15,12,16,
                8,12,9,  12,9,13,
                9,13,10,  13,10,14,
                10,14,15,  10,15,11,
                11,15,8,  15,8,12,
                0,1,4,  1,4,5,
                1,5,2,  5,2,6,
                6,2,7,  2,7,3,
                3,7,4,  3,4,0,
                4,8,9,  4,9,5,
                5,9,10,  5,10,6,
                10,6,11,  6,11,7,
                11,7,4,  11,4,8,
            };
            mesh.Indices.insert(mesh.Indices.end(), arr, arr+sizeof(arr)/sizeof(arr[0]));
            mesh.Normals.push_back(VM::vec3(0, 0, -1));
            mesh.Normals.push_back(VM::vec3(-1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, 0, 1));
            mesh.Normals.push_back(VM::vec3(1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, 0.24, -0.97));
            mesh.Normals.push_back(VM::vec3(-1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, -0.24, 0.97));
            mesh.Normals.push_back(VM::vec3(1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, 0.47, -0.88));
            mesh.Normals.push_back(VM::vec3(-1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, -0.47, 0.88));
            mesh.Normals.push_back(VM::vec3(1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, 0.67, -0.74));
            mesh.Normals.push_back(VM::vec3(-1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, -0.67, 0.74));
            mesh.Normals.push_back(VM::vec3(1, 0, 0));
            mesh.Normals.push_back(VM::vec3(0, 0.83, -0.56));
            break;
        }
        case GROUND: {
            mesh.Normals.reserve((MapSize+1)*(MapSize+1));
            float step = 1.0/MapSize;
            for (uint i=0; i<=MapSize; ++i) {
                for (uint j=0; j<=MapSize; j++) {
                    mesh.Normals[(i)*(MapSize+1)+(j)] = VM::vec3(0,0,0);
                }
            }
            for (uint i=0; i<=MapSize; ++i) {
                mesh.Points.push_back(VM::vec4(0,S.heightMap[0][i],i*step,1));
            }
            for (uint i=1; i<=MapSize; i++) {
                float x=i*step;
                mesh.Points.push_back(VM::vec4(x,S.heightMap[i][0],0,1));
                for (uint j=1; j<=MapSize; j++) {
                    float z=j*step;
                    mesh.Points.push_back(VM::vec4(x,S.heightMap[i][j],z,1));

                    mesh.Indices.push_back((i-1)*(MapSize+1)+(j-1));
                    mesh.Indices.push_back((i-1)*(MapSize+1)+(j));
                    mesh.Indices.push_back((i)*(MapSize+1)+(j-1));

                    mesh.Indices.push_back((i)*(MapSize+1)+(j-1));
                    mesh.Indices.push_back((i-1)*(MapSize+1)+(j));
                    mesh.Indices.push_back((i)*(MapSize+1)+(j));

                    VM::vec3 n1, n2;
                    float y1 = S.heightMap[i-1][j-1];
                    float y2 = S.heightMap[i][j-1];
                    float y3 = S.heightMap[i-1][j];
                    float y4 = S.heightMap[i][j];
                    n1 = VM::vec3(-(y2-y1)/step, 1, -(y3-y1)/step);
                    n2 = VM::vec3((y3-y4)/step, 1, (y2-y4)/step);
                    mesh.Normals[(i-1)*(MapSize+1)+(j-1)] += n1;
                    mesh.Normals[(i)*(MapSize+1)+(j-1)] += n1+n2;
                    mesh.Normals[(i-1)*(MapSize+1)+(j)] += n1+n2;
                    mesh.Normals[(i)*(MapSize+1)+(j)] += n2;
                }
            }
            break;
        }
        case ROCK: {
            loadObject(mesh, &pRockTexture, "Rock1/", "Rock1.obj", 1, 1);
            break;
        }
        case LAMP: {
            mesh.Points.push_back(VM::vec4(0.2,0,0.2,1));
            mesh.Points.push_back(VM::vec4(0.8,0,0.2,1));
            mesh.Points.push_back(VM::vec4(0.8,1,0.2,1));
            mesh.Points.push_back(VM::vec4(0.2,1,0.2,1));
            mesh.Normals.push_back(VM::vec3(0,0,-1));
            mesh.Normals.push_back(VM::vec3(0,0,-1));
            mesh.Normals.push_back(VM::vec3(0,0,-1));
            mesh.Normals.push_back(VM::vec3(0,0,-1));

            mesh.Points.push_back(VM::vec4(0.8,0,0.2,1));
            mesh.Points.push_back(VM::vec4(0.8,0,0.8,1));
            mesh.Points.push_back(VM::vec4(0.8,1,0.8,1));
            mesh.Points.push_back(VM::vec4(0.8,1,0.2,1));
            mesh.Normals.push_back(VM::vec3(1,0,0));
            mesh.Normals.push_back(VM::vec3(1,0,0));
            mesh.Normals.push_back(VM::vec3(1,0,0));
            mesh.Normals.push_back(VM::vec3(1,0,0));

            mesh.Points.push_back(VM::vec4(0.8,0,0.8,1));
            mesh.Points.push_back(VM::vec4(0.2,0,0.8,1));
            mesh.Points.push_back(VM::vec4(0.2,1,0.8,1));
            mesh.Points.push_back(VM::vec4(0.8,1,0.8,1));
            mesh.Normals.push_back(VM::vec3(0,0,1));
            mesh.Normals.push_back(VM::vec3(0,0,1));
            mesh.Normals.push_back(VM::vec3(0,0,1));
            mesh.Normals.push_back(VM::vec3(0,0,1));

            mesh.Points.push_back(VM::vec4(0.2,0,0.8,1));
            mesh.Points.push_back(VM::vec4(0.2,0,0.2,1));
            mesh.Points.push_back(VM::vec4(0.2,1,0.2,1));
            mesh.Points.push_back(VM::vec4(0.2,1,0.8,1));
            mesh.Normals.push_back(VM::vec3(-1,0,0));
            mesh.Normals.push_back(VM::vec3(-1,0,0));
            mesh.Normals.push_back(VM::vec3(-1,0,0));
            mesh.Normals.push_back(VM::vec3(-1,0,0));

            uint arr[] = {
                0,1,2,  0,3,2,
                4,5,6,  4,7,6,
                8,9,10, 8,11,10,
                12,13,14, 12,15,14
            };
            mesh.Indices.insert(mesh.Indices.end(), arr, arr+sizeof(arr)/sizeof(arr[0]));
            break;
        }
        case BORDER: {
            mesh.Points.push_back(VM::vec4(0.19,0,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,0,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,0,0.81,1));
            mesh.Points.push_back(VM::vec4(0.19,0,0.81,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.81,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.81,1));
            mesh.Normals.push_back(VM::vec3(-1,-1,-1));
            mesh.Normals.push_back(VM::vec3(1,-1,-1));
            mesh.Normals.push_back(VM::vec3(1,-1,1));
            mesh.Normals.push_back(VM::vec3(-1,-1,1));
            mesh.Normals.push_back(VM::vec3(-1,1,-1));
            mesh.Normals.push_back(VM::vec3(1,1,-1));
            mesh.Normals.push_back(VM::vec3(1,1,1));
            mesh.Normals.push_back(VM::vec3(-1,1,1));

            mesh.Points.push_back(VM::vec4(0.21,0,0.19,1));
            mesh.Points.push_back(VM::vec4(0.79,0,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,0,0.21,1));
            mesh.Points.push_back(VM::vec4(0.81,0,0.79,1));
            mesh.Points.push_back(VM::vec4(0.79,0,0.81,1));
            mesh.Points.push_back(VM::vec4(0.21,0,0.81,1));
            mesh.Points.push_back(VM::vec4(0.19,0,0.79,1));
            mesh.Points.push_back(VM::vec4(0.19,0,0.21,1));
            mesh.Points.push_back(VM::vec4(0.21,1,0.19,1));
            mesh.Points.push_back(VM::vec4(0.79,1,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.21,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.79,1));
            mesh.Points.push_back(VM::vec4(0.79,1,0.81,1));
            mesh.Points.push_back(VM::vec4(0.21,1,0.81,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.79,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.21,1));
            mesh.Normals.push_back(VM::vec3(0,0,-1));
            mesh.Normals.push_back(VM::vec3(0,0,-1));
            mesh.Normals.push_back(VM::vec3(1,0,0));
            mesh.Normals.push_back(VM::vec3(1,0,0));
            mesh.Normals.push_back(VM::vec3(0,0,1));
            mesh.Normals.push_back(VM::vec3(0,0,1));
            mesh.Normals.push_back(VM::vec3(-1,0,0));
            mesh.Normals.push_back(VM::vec3(-1,0,0));
            mesh.Normals.push_back(VM::vec3(0,0,-1));
            mesh.Normals.push_back(VM::vec3(0,0,-1));
            mesh.Normals.push_back(VM::vec3(1,0,0));
            mesh.Normals.push_back(VM::vec3(1,0,0));
            mesh.Normals.push_back(VM::vec3(0,0,1));
            mesh.Normals.push_back(VM::vec3(0,0,1));
            mesh.Normals.push_back(VM::vec3(-1,0,0));

            mesh.Points.push_back(VM::vec4(0.19,0.2,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,0.2,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,0.2,0.81,1));
            mesh.Points.push_back(VM::vec4(0.19,0.2,0.81,1));
            mesh.Normals.push_back(VM::vec3(-1,0,-1));
            mesh.Normals.push_back(VM::vec3(1,0,-1));
            mesh.Normals.push_back(VM::vec3(1,0,1));
            mesh.Normals.push_back(VM::vec3(-1,0,1));

            mesh.Points.push_back(VM::vec4(0.5,1.3,0.5,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.19,1));
            mesh.Normals.push_back(VM::vec3(0,1,-1));
            mesh.Normals.push_back(VM::vec3(0,1,-1));
            mesh.Normals.push_back(VM::vec3(0,1,-1));

            mesh.Points.push_back(VM::vec4(0.5,1.3,0.5,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.19,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.81,1));
            mesh.Normals.push_back(VM::vec3(1,1,0));
            mesh.Normals.push_back(VM::vec3(1,1,0));
            mesh.Normals.push_back(VM::vec3(1,1,0));

            mesh.Points.push_back(VM::vec4(0.5,1.3,0.5,1));
            mesh.Points.push_back(VM::vec4(0.81,1,0.81,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.81,1));
            mesh.Normals.push_back(VM::vec3(0,1,1));
            mesh.Normals.push_back(VM::vec3(0,1,1));
            mesh.Normals.push_back(VM::vec3(0,1,1));

            mesh.Points.push_back(VM::vec4(0.5,1.3,0.5,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.19,1));
            mesh.Points.push_back(VM::vec4(0.19,1,0.81,1));
            mesh.Normals.push_back(VM::vec3(-1,1,0));
            mesh.Normals.push_back(VM::vec3(-1,1,0));
            mesh.Normals.push_back(VM::vec3(-1,1,0));
            uint arr[] = {
                15,0,4,    15,23,4,
                8,0,4,     8,16,4,
                9,1,5,     9,17,5,
                10,1,5,    10,18,5,
                11,2,6,    11,19,6,
                12,2,6,    12,20,6,
                13,3,7,    13,21,7,
                14,3,7,    14,22,7,

                0,1,25,    0,24,25,
                1,2,26,    1,25,26,
                2,3,27,    2,26,27,
                3,0,24,    3,27,24,

                28,29,30,
                31,32,33,
                34,35,36,
                37,38,39
            };
            mesh.Indices.insert(mesh.Indices.end(), arr, arr+sizeof(arr)/sizeof(arr[0]));
            break;
        }
        case LAKE: {
            mesh.Points.push_back(VM::vec4(0.4,0,0.3,1));
            mesh.Points.push_back(VM::vec4(1,0,0.3,1));
            mesh.Points.push_back(VM::vec4(1,0,0.9,1));
            mesh.Points.push_back(VM::vec4(0.4,0,0.9,1));
            mesh.Normals.push_back(VM::vec3(0,1,0));
            mesh.Normals.push_back(VM::vec3(0,1,0));
            mesh.Normals.push_back(VM::vec3(0,1,0));
            mesh.Normals.push_back(VM::vec3(0,1,0));
            uint arr[] = {
                0,1,2,   0,3,2
            };
            mesh.Indices.insert(mesh.Indices.end(), arr, arr+sizeof(arr)/sizeof(arr[0]));
            break;
        }
    }
}

// Создание травы
void CreateGrass() {
    // Создаём меш
    Mesh mesh;
    GenMesh(mesh, GRASS);
    // Сохраняем количество вершин в меше травы
    grassPointsCount = mesh.Indices.size();
    /* Компилируем шейдеры
    Эта функция принимает на вход название шейдера 'shaderName',
    читает файлы shaders/{shaderName}.vert - вершинный шейдер
    и shaders/{shaderName}.frag - фрагментный шейдер,
    компилирует их и линкует.
    */
    grassShader = GL::CompileShaderProgram("grass");

    // Здесь создаём буфер
    /*GLuint pointsBuffer;
    // Это генерация одного буфера (в pointsBuffer хранится идентификатор буфера)
    glGenBuffers(1, &pointsBuffer);                                              CHECK_GL_ERRORS
    // Привязываем сгенерированный буфер
    glBindBuffer(GL_ARRAY_BUFFER, pointsBuffer);                                 CHECK_GL_ERRORS
    // Заполняем буфер данными из вектора
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * grassPoints.size(), grassPoints.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS
*/
    // Создание VAO
    // Генерация VAO
    glGenVertexArrays(1, &grassVAO);                                             CHECK_GL_ERRORS
    // Привязка VAO
    glBindVertexArray(grassVAO);                                                 CHECK_GL_ERRORS

    GLuint Buffer[2];
    glGenBuffers(2, Buffer);
    glBindBuffer(GL_ARRAY_BUFFER, Buffer[0]);
    glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*mesh.Points.size(), mesh.Points.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,0,0);

    /*glBindBuffer(GL_ARRAY_BUFFER, Buffer[1]);
    glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec3)*grassNormals.size(), grassNormals.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,0);*/

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint)*mesh.Indices.size(),mesh.Indices.data(),GL_STATIC_DRAW);

    // Получение локации параметра 'point' в шейдере
    GLuint pointsLocation = glGetAttribLocation(grassShader, "point");           CHECK_GL_ERRORS
    // Подключаем массив атрибутов к данной локации
    glEnableVertexAttribArray(pointsLocation);                                   CHECK_GL_ERRORS
    // Устанавливаем параметры для получения данных из массива (по 4 значение типа float на одну вершину)
    glVertexAttribPointer(pointsLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    GLuint normalsBuffer;
    glGenBuffers(1, &normalsBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3)*mesh.Normals.size(), mesh.Normals.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint normalsLocation = glGetAttribLocation(grassShader, "normal");          CHECK_GL_ERRORS
    glEnableVertexAttribArray(normalsLocation);                                   CHECK_GL_ERRORS
    glVertexAttribPointer(normalsLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    // Создаём буфер для позиций травинок
    glGenBuffers(1, &positionBuffer);                                            CHECK_GL_ERRORS
    // Здесь мы привязываем новый буфер, так что дальше вся работа будет с ним до следующего вызова glBindBuffer
    glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3) * S.grassPositions.size(), S.grassPositions.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint positionLocation = glGetAttribLocation(grassShader, "position");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(positionLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    // Здесь мы указываем, что нужно брать новое значение из этого буфера для каждого инстанса (для каждой травинки)
    glVertexAttribDivisor(positionLocation, 1);                                  CHECK_GL_ERRORS

    glGenBuffers(1, &scaleBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, scaleBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3) * S.grassScale.size(), S.grassScale.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint scaleLocation = glGetAttribLocation(grassShader, "grassScale");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(scaleLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(scaleLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    glVertexAttribDivisor(scaleLocation, 1);                                  CHECK_GL_ERRORS

    glGenBuffers(1, &alphaBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, alphaBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * S.grassAlpha.size(), S.grassAlpha.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint alphaLocation = glGetAttribLocation(grassShader, "alpha");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(alphaLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(alphaLocation, 1, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    glVertexAttribDivisor(alphaLocation, 1);                                  CHECK_GL_ERRORS

    // Создаём буфер для смещения травинок
    glGenBuffers(1, &grassVariance);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, grassVariance);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * S.grassVarianceData.size(), S.grassVarianceData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint varianceLocation = glGetAttribLocation(grassShader, "variance");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(varianceLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(varianceLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    glVertexAttribDivisor(varianceLocation, 1);                                  CHECK_GL_ERRORS

/*    GLuint normalsBuffer;
    glGenBuffers(1, &normalsBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3) * grassNormals.size(), grassNormals.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint normalsLocation = glGetAttribLocation(grassShader, "normal");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(normalsLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(normalsLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    glVertexAttribDivisor(normalsLocation, 1);                                  CHECK_GL_ERRORS
*/




    pGrassTexture = new Texture(GL_TEXTURE_2D, "Texture/grass.jpg");
    pGrassTexture->Load();

    // Отвязываем VAO
    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    // Отвязываем буфер
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

// Создаём камеру (Если шаблонная камера вам не нравится, то можете переделать, но я бы не стал)
void CreateCamera() {
    camera.angle = 45.0f / 180.0f * M_PI;
    camera.direction = VM::vec3(0, 0.3, -1);
    camera.position = VM::vec3(0.5, 0.2, 0);
    camera.screenRatio = (float)screenWidth / screenHeight;
    camera.up = VM::vec3(0, 1, 0);
    camera.zfar = 50.0f;
    camera.znear = 0.05f;
}

// Создаём замлю
void CreateGround() {
    // Земля состоит из двух треугольников
    GenMesh(groundMesh, GROUND);

    // Подробнее о том, как это работает читайте в функции CreateGrass

    groundShader = GL::CompileShaderProgram("ground");

    /*GLuint pointsBuffer;
    glGenBuffers(1, &pointsBuffer);                                              CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, pointsBuffer);                                 CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * meshPoints.size(), meshPoints.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS
*/
    glGenVertexArrays(1, &groundVAO);                                            CHECK_GL_ERRORS
    glBindVertexArray(groundVAO);                                                CHECK_GL_ERRORS

    GLuint Buffer;
    glGenBuffers(1, &Buffer);
    glGenBuffers(1, &groundBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, groundBuffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*groundMesh.Points.size(), groundMesh.Points.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,0,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint)*groundMesh.Indices.size(),groundMesh.Indices.data(),GL_STATIC_DRAW);

    GLuint index = glGetAttribLocation(groundShader, "point");                   CHECK_GL_ERRORS
    glEnableVertexAttribArray(index);                                            CHECK_GL_ERRORS
    glVertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, 0, 0);                   CHECK_GL_ERRORS

    GLuint normalsBuffer;
    glGenBuffers(1, &normalsBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3)*(MapSize+1)*(MapSize+1), groundMesh.Normals.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint normalsLocation = glGetAttribLocation(groundShader, "normal");          CHECK_GL_ERRORS
    glEnableVertexAttribArray(normalsLocation);                                   CHECK_GL_ERRORS
    glVertexAttribPointer(normalsLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    pGroundTexture = new Texture(GL_TEXTURE_2D, "Texture/ground.jpg");
    pGroundTexture->Load();

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

void CreateRock() {
    Mesh mesh;
    GenMesh(mesh, ROCK);

    rockPointsCount = mesh.Indices.size();

    rockShader = GL::CompileShaderProgram("rock");

    glGenVertexArrays(1, &rockVAO);                                            CHECK_GL_ERRORS
    glBindVertexArray(rockVAO);                                                CHECK_GL_ERRORS

    GLuint Buffer[2];
    glGenBuffers(2, Buffer);
    glBindBuffer(GL_ARRAY_BUFFER, Buffer[0]);
    glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*mesh.Points.size(), mesh.Points.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,0,0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint)*mesh.Indices.size(),mesh.Indices.data(),GL_STATIC_DRAW);

    GLuint index = glGetAttribLocation(rockShader, "point");                   CHECK_GL_ERRORS
    glEnableVertexAttribArray(index);                                            CHECK_GL_ERRORS
    glVertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, 0, 0);                   CHECK_GL_ERRORS

    GLuint normalsBuffer;
    glGenBuffers(1, &normalsBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3)*mesh.Normals.size(), mesh.Normals.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint normalsLocation = glGetAttribLocation(rockShader, "normal");          CHECK_GL_ERRORS
    glEnableVertexAttribArray(normalsLocation);                                   CHECK_GL_ERRORS
    glVertexAttribPointer(normalsLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    GLuint texBuffer;
    glGenBuffers(1, &texBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, texBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec2)*mesh.TexCoords.size(), mesh.TexCoords.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint texLocation = glGetAttribLocation(rockShader, "TexCoord");          CHECK_GL_ERRORS
    glEnableVertexAttribArray(texLocation);                                   CHECK_GL_ERRORS
    glVertexAttribPointer(texLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

void CreateLamp() {
    Mesh mesh;
    GenMesh(mesh, LAMP);
    lampPointsCount = mesh.Indices.size();

    lampShader = GL::CompileShaderProgram("lamp");

    glGenVertexArrays(1, &lampVAO);                                            CHECK_GL_ERRORS
    glBindVertexArray(lampVAO);                                                CHECK_GL_ERRORS

    GLuint Buffer[2];
    glGenBuffers(2, Buffer);                                                    CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, Buffer[0]);                                   CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*mesh.Points.size(), mesh.Points.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    glEnableVertexAttribArray(0);                                               CHECK_GL_ERRORS
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,0,0);                           CHECK_GL_ERRORS
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer[1]);                           CHECK_GL_ERRORS
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint)*mesh.Indices.size(),mesh.Indices.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint index = glGetAttribLocation(lampShader, "point");                   CHECK_GL_ERRORS
    glEnableVertexAttribArray(index);                                            CHECK_GL_ERRORS
    glVertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, 0, 0);                   CHECK_GL_ERRORS

    GLuint normalsBuffer;
    glGenBuffers(1, &normalsBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3)*mesh.Normals.size(), mesh.Normals.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint normalsLocation = glGetAttribLocation(lampShader, "normal");          CHECK_GL_ERRORS
    glEnableVertexAttribArray(normalsLocation);                                   CHECK_GL_ERRORS
    glVertexAttribPointer(normalsLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

void CreateBorder() {
    Mesh mesh;
    GenMesh(mesh, BORDER);
    borderPointsCount = mesh.Indices.size();

    borderShader = GL::CompileShaderProgram("border");

    glGenVertexArrays(1, &borderVAO);                                            CHECK_GL_ERRORS
    glBindVertexArray(borderVAO);                                                CHECK_GL_ERRORS

    GLuint Buffer[2];
    glGenBuffers(2, Buffer);                                                    CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, Buffer[0]);                                   CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*mesh.Points.size(), mesh.Points.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    glEnableVertexAttribArray(0);                                               CHECK_GL_ERRORS
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,0,0);                           CHECK_GL_ERRORS
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer[1]);                           CHECK_GL_ERRORS
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint)*mesh.Indices.size(),mesh.Indices.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint index = glGetAttribLocation(borderShader, "point");                   CHECK_GL_ERRORS
    glEnableVertexAttribArray(index);                                            CHECK_GL_ERRORS
    glVertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, 0, 0);                   CHECK_GL_ERRORS

    GLuint normalsBuffer;
    glGenBuffers(1, &normalsBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3)*mesh.Normals.size(), mesh.Normals.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint normalsLocation = glGetAttribLocation(borderShader, "normal");          CHECK_GL_ERRORS
    glEnableVertexAttribArray(normalsLocation);                                   CHECK_GL_ERRORS
    glVertexAttribPointer(normalsLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

void CreateLake() {
    Mesh mesh;
    GenMesh(mesh, LAKE);
    lakePointsCount = mesh.Indices.size();

    lakeShader = GL::CompileShaderProgram("lake");

    glGenVertexArrays(1, &lakeVAO);                                            CHECK_GL_ERRORS
    glBindVertexArray(lakeVAO);                                                CHECK_GL_ERRORS

    GLuint Buffer[2];
    glGenBuffers(2, Buffer);                                                    CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, Buffer[0]);                                   CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER,sizeof(VM::vec4)*mesh.Points.size(), mesh.Points.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    glEnableVertexAttribArray(0);                                               CHECK_GL_ERRORS
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,0,0);                           CHECK_GL_ERRORS
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer[1]);                           CHECK_GL_ERRORS
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(uint)*mesh.Indices.size(),mesh.Indices.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint index = glGetAttribLocation(lakeShader, "point");                   CHECK_GL_ERRORS
    glEnableVertexAttribArray(index);                                            CHECK_GL_ERRORS
    glVertexAttribPointer(index, 4, GL_FLOAT, GL_FALSE, 0, 0);                   CHECK_GL_ERRORS

    GLuint normalsBuffer;
    glGenBuffers(1, &normalsBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, normalsBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec3)*mesh.Normals.size(), mesh.Normals.data(),GL_STATIC_DRAW); CHECK_GL_ERRORS
    GLuint normalsLocation = glGetAttribLocation(lakeShader, "normal");          CHECK_GL_ERRORS
    glEnableVertexAttribArray(normalsLocation);                                   CHECK_GL_ERRORS
    glVertexAttribPointer(normalsLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS

    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

int main(int argc, char **argv)
{
    mytimer = clock();
    try {
        putenv("MESA_GL_VERSION_OVERRIDE=3.3COMPAT");
        cout << "Start" << endl;
        InitializeGLUT(argc, argv);
        cout << "GLUT inited" << endl;
        glewInit();
        cout << "glew inited" << endl;
        CreateCamera();
        cout << "Camera created" << endl;
        CreateGround();
        cout << "Ground created" << endl;
        CreateGrass();
        cout << "Grass created" << endl;
        CreateRock();
        cout << "Rock created" << endl;
        CreateLamp();
        cout << "Lamp created" << endl;
        CreateBorder();
        cout << "Border created" << endl;
        CreateLake();
        cout << "Lake created" << endl;
        glutMainLoop();
    } catch (string s) {
        cout << s << endl;
    }
}
