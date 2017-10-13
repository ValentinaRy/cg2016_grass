#include "Utility.h"
#include <cstdio>

using namespace std;

class Serializer {
public:
    virtual bool InOut(float&) = 0;
    virtual bool InOut(VM::vec4&) = 0;
    virtual bool InOut(VM::vec3&) = 0;
    virtual bool InOut(VM::vec2&) = 0;
    virtual bool InOut(bool&) = 0;
    virtual bool InOut(int&) = 0;
};

class BinarySerializerReader : public Serializer {
    string name;
    FILE *file;
public:
    BinarySerializerReader(const string& name): name(name){}
    bool Open() {
        return ((file = fopen(name.c_str(), "rb")) != NULL);
    }
    bool Close() {
        return (fclose(file) != EOF);
    }
    bool InOut(float &val) {
        return fread(&val,sizeof(val),1,file);
    }
    bool InOut(VM::vec2 &val) {
        return fread(&val,sizeof(float),2,file);
    }
    bool InOut(VM::vec3 &val) {
        return fread(&val,sizeof(float),3,file);
    }
    bool InOut(VM::vec4 &val) {
        return fread(&val,sizeof(float),4,file);
    }
    bool InOut(bool &val) {
        return fread(&val,sizeof(val),1,file);
    }
    bool InOut(int &val) {
        return fread(&val,sizeof(int),1,file);
    }
};

class BinarySerializerWriter : public Serializer {
    string name;
    FILE *file;
public:
    BinarySerializerWriter(const string& name): name(name){}
    bool Open() {
        return ((file = fopen(name.c_str(), "wb")) != NULL);
    }
    bool Close() {
        return (fclose(file) != EOF);
    }
    bool InOut(float &val) {
        return fwrite(&val,sizeof(val),1,file);
    }
    bool InOut(VM::vec2 &val) {
        return fwrite(&val,sizeof(float),2,file);
    }
    bool InOut(VM::vec3 &val) {
        return fwrite(&val,sizeof(float),3,file);
    }
    bool InOut(VM::vec4 &val) {
        return fwrite(&val,sizeof(float),4,file);
    }
    bool InOut(bool &val) {
        return fwrite(&val,sizeof(val),1,file);
    }
    bool InOut(int &val) {
        return fwrite(&val,sizeof(int),1,file);
    }
};

class TxtSerializerReader : public Serializer {
    string name;
    FILE *file;
public:
    TxtSerializerReader(const string& name): name(name){}
    bool Open() {
        return ((file = fopen(name.c_str(), "r")) != NULL);
    }
    bool Close() {
        return (fclose(file) != EOF);
    }
    bool InOut(float &val) {
        return (fscanf(file, "%f", &val) != EOF);
    }
    bool InOut(VM::vec2 &val) {
        return (fscanf(file, "%f%f", &val.x, &val.y) != EOF);
    }
    bool InOut(VM::vec3 &val) {
        return (fscanf(file, "%f%f%f", &val.x,&val.y,&val.z) != EOF);
    }
    bool InOut(VM::vec4 &val) {
        return (fscanf(file, "%f%f%f%f", &val.x,&val.y,&val.z,&val.w) != EOF);
    }
    bool InOut(bool &val) {
        int x;
        if (fscanf(file, "%d", &x) == EOF) {
            return 0;
        }
        val = x;
        return 1;
    }
    bool InOut(int &val) {
        return (fscanf(file, "%d", &val) != EOF);
    }
};

class TxtSerializerWriter : public Serializer {
    string name;
    FILE *file;
public:
    TxtSerializerWriter(const string& name): name(name){}
    bool Open() {
        return ((file = fopen(name.c_str(), "w")) != NULL);
    }
    bool Close() {
        return (fclose(file) != EOF);
    }
    bool InOut(float &val) {
        return (fprintf(file, "%f\n", val) != EOF);
    }
    bool InOut(VM::vec2 &val) {
        return (fprintf(file, "%f\n%f\n", val.x, val.y) != EOF);
    }
    bool InOut(VM::vec3 &val) {
        return (fprintf(file, "%f\n%f\n%f\n", val.x,val.y,val.z) != EOF);
    }
    bool InOut(VM::vec4 &val) {
        return (fprintf(file, "%f\n%f\n%f\n%f\n", val.x,val.y,val.z,val.w) != EOF);
    }
    bool InOut(bool &val) {
        return (fprintf(file, "%d\n", val) != EOF);
    }
    bool InOut(int &val) {
        return (fprintf(file, "%d\n", val) != EOF);
    }
};
