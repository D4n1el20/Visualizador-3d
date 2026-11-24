// Leitor de OBJ/MTL independente de OpenGL (so geometria e materiais).
#ifndef OBJMODEL_H
#define OBJMODEL_H

#include <string>
#include <vector>

struct ObjMaterial {
    std::string name;
    float kd[3];            // cor difusa do .mtl
    std::string mapKdPath;  // caminho completo da textura difusa (vazio se nao houver)
    unsigned int texId;     // textura OpenGL (0 = sem textura); preenchido pelo main
    ObjMaterial() : texId(0) { kd[0] = kd[1] = kd[2] = 0.8f; }
};

// Faixa de vertices que usa um mesmo material
struct ObjPart {
    int material;
    int firstVertex;
    int vertexCount;
};

class ObjModel {
public:
    ObjModel();
    bool load(const std::string& filePath, std::string& error);
    void clear();
    int triangleCount() const { return (int)(pos.size() / 9); }
    int vertexCount() const { return (int)(pos.size() / 3); }  // vertices expandidos (3 por triangulo)

    // Arrays expandidos: 3 vertices por triangulo, prontos para glDrawArrays
    std::vector<float> pos;            // x y z
    std::vector<float> smoothNormals;  // normal por vertice (Smooth)
    std::vector<float> flatNormals;    // normal da face (Flat)
    std::vector<float> uv;             // u v
    std::vector<ObjMaterial> materials;
    std::vector<ObjPart> parts;

    bool hasFileUV;        // arquivo tinha vt
    bool hasFileNormals;   // arquivo tinha vn
    bool uvGenerated;      // alguma UV foi gerada automaticamente
    int srcVertices, srcUVs, srcNormals;
    std::string path, dir;
};

#endif
