#define _CRT_SECURE_NO_WARNINGS
#include "ObjModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

const float kPi = 3.14159265358979f;

struct Corner { int v, t, n; };
struct Tri { Corner c[3]; int mat; };

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string fixSlashes(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) if (s[i] == '\\') s[i] = '/';
    return s;
}

std::string dirOf(const std::string& p) {
    size_t i = p.find_last_of("/\\");
    return i == std::string::npos ? std::string() : p.substr(0, i + 1);
}

std::string baseOf(const std::string& p) {
    size_t i = p.find_last_of("/\\");
    return i == std::string::npos ? p : p.substr(i + 1);
}

bool fileExists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool isAbsolute(const std::string& p) {
    return (p.size() > 1 && p[1] == ':') || (!p.empty() && p[0] == '/');
}

// Converte indice do OBJ (base 1, ou negativo = relativo) para base 0. -1 se invalido.
int resolveIndex(int idx, int count) {
    if (idx > 0) return idx <= count ? idx - 1 : -1;
    if (idx < 0) return count + idx >= 0 ? count + idx : -1;
    return -1;
}

// "v", "v/vt", "v//vn" ou "v/vt/vn"
bool parseCorner(const std::string& tok, int nv, int nt, int nn, Corner& c) {
    int idx[3] = {0, 0, 0};
    bool has[3] = {false, false, false};
    int field = 0;
    std::string cur;
    for (size_t k = 0; k <= tok.size(); ++k) {
        char ch = k < tok.size() ? tok[k] : '/';
        if (ch == '/') {
            if (field < 3 && !cur.empty()) { idx[field] = atoi(cur.c_str()); has[field] = true; }
            cur.clear();
            ++field;
        } else {
            cur += ch;
        }
    }
    if (!has[0]) return false;
    c.v = resolveIndex(idx[0], nv);
    c.t = has[1] ? resolveIndex(idx[1], nt) : -1;
    c.n = has[2] ? resolveIndex(idx[2], nn) : -1;
    return c.v >= 0;
}

void parseMtl(const std::string& file, const std::string& dir, std::vector<ObjMaterial>& mats) {
    std::ifstream in(file.c_str());
    if (!in) return;
    std::string line;
    int cur = -1;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t sp = line.find_first_of(" \t");
        std::string key = sp == std::string::npos ? line : line.substr(0, sp);
        std::string rest = sp == std::string::npos ? "" : trim(line.substr(sp));
        if (key == "newmtl") {
            ObjMaterial m;
            m.name = rest;
            mats.push_back(m);
            cur = (int)mats.size() - 1;
        } else if (cur < 0) {
            continue;
        } else if (key == "Kd") {
            sscanf(rest.c_str(), "%f %f %f", &mats[cur].kd[0], &mats[cur].kd[1], &mats[cur].kd[2]);
        } else if (key == "map_Kd") {
            // ignora opcoes do tipo "-s 1 1 1": fica com o ultimo token se comecar com '-'
            if (!rest.empty() && rest[0] == '-') {
                size_t last = rest.find_last_of(" \t");
                if (last != std::string::npos) rest = trim(rest.substr(last));
            }
            rest = fixSlashes(rest);
            std::string full = isAbsolute(rest) ? rest : dir + rest;
            if (!fileExists(full)) full = dir + baseOf(rest);  // tenta so o nome na pasta do OBJ
            mats[cur].mapKdPath = full;
        }
    }
}

int findMaterial(const std::vector<ObjMaterial>& mats, const std::string& name) {
    for (size_t i = 0; i < mats.size(); ++i) if (mats[i].name == name) return (int)i;
    return -1;
}

void cross(const float* a, const float* b, float* o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

// normaliza; retorna false se o vetor e (quase) nulo
bool normalize(float* v) {
    float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l < 1e-12f) return false;
    v[0] /= l; v[1] /= l; v[2] /= l;
    return true;
}

} // namespace

ObjModel::ObjModel() { clear(); }

void ObjModel::clear() {
    pos.clear(); smoothNormals.clear(); flatNormals.clear(); uv.clear();
    materials.clear(); parts.clear();
    hasFileUV = hasFileNormals = uvGenerated = false;
    srcVertices = srcUVs = srcNormals = 0;
    path.clear(); dir.clear();
}

bool ObjModel::load(const std::string& filePath, std::string& error) {
    clear();
    std::ifstream in(filePath.c_str());
    if (!in) { error = "nao foi possivel abrir o arquivo"; return false; }
    path = filePath;
    dir = dirOf(fixSlashes(filePath));

    std::vector<float> V, T, N;
    std::vector<Tri> tris;
    int curMat = -1;
    std::string line;

    while (std::getline(in, line)) {
        if (line.size() < 2) continue;
        const char* s = line.c_str();
        if (s[0] == 'v' && s[1] == ' ') {
            float x, y, z;
            if (sscanf(s + 2, "%f %f %f", &x, &y, &z) == 3) { V.push_back(x); V.push_back(y); V.push_back(z); }
        } else if (s[0] == 'v' && s[1] == 't') {
            float u, v;
            if (sscanf(s + 3, "%f %f", &u, &v) == 2) { T.push_back(u); T.push_back(v); }
        } else if (s[0] == 'v' && s[1] == 'n') {
            float x, y, z;
            if (sscanf(s + 3, "%f %f %f", &x, &y, &z) == 3) { N.push_back(x); N.push_back(y); N.push_back(z); }
        } else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t')) {
            std::istringstream ss(s + 2);
            std::string tok;
            std::vector<Corner> poly;
            while (ss >> tok) {
                Corner c;
                if (!parseCorner(tok, (int)V.size() / 3, (int)T.size() / 2, (int)N.size() / 3, c)) {
                    error = "indice invalido em uma face: " + tok;
                    return false;
                }
                poly.push_back(c);
            }
            for (size_t i = 1; i + 1 < poly.size(); ++i) {  // triangulacao em leque
                Tri t;
                t.c[0] = poly[0]; t.c[1] = poly[i]; t.c[2] = poly[i + 1];
                t.mat = curMat;
                tris.push_back(t);
            }
        } else if (strncmp(s, "mtllib", 6) == 0) {
            std::string name = fixSlashes(trim(s + 6));
            if (!name.empty()) parseMtl(isAbsolute(name) ? name : dir + name, dir, materials);
        } else if (strncmp(s, "usemtl", 6) == 0) {
            std::string name = trim(s + 6);
            int m = findMaterial(materials, name);
            if (m < 0) { ObjMaterial nm; nm.name = name; materials.push_back(nm); m = (int)materials.size() - 1; }
            curMat = m;
        }
    }

    if (V.empty() || tris.empty()) { error = "o arquivo nao tem vertices/faces validos"; return false; }

    // faces sem usemtl -> material padrao
    bool needDefault = false;
    for (size_t i = 0; i < tris.size(); ++i) if (tris[i].mat < 0) { needDefault = true; break; }
    if (needDefault) {
        int d = findMaterial(materials, "default");
        if (d < 0) { ObjMaterial dm; dm.name = "default"; materials.push_back(dm); d = (int)materials.size() - 1; }
        for (size_t i = 0; i < tris.size(); ++i) if (tris[i].mat < 0) tris[i].mat = d;
    }

    srcVertices = (int)V.size() / 3;
    srcUVs = (int)T.size() / 2;
    srcNormals = (int)N.size() / 3;
    hasFileUV = !T.empty();
    hasFileNormals = !N.empty();

    // caixa envolvente dos vertices usados -> centraliza e escala para caber em [-1,1]
    float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i < tris.size(); ++i)
        for (int k = 0; k < 3; ++k) {
            const float* p = &V[tris[i].c[k].v * 3];
            for (int a = 0; a < 3; ++a) { if (p[a] < mn[a]) mn[a] = p[a]; if (p[a] > mx[a]) mx[a] = p[a]; }
        }
    float ctr[3], ext = 0;
    for (int a = 0; a < 3; ++a) { ctr[a] = 0.5f * (mn[a] + mx[a]); if (mx[a] - mn[a] > ext) ext = mx[a] - mn[a]; }
    float scale = ext > 1e-12f ? 2.0f / ext : 1.0f;

    // normais por vertice calculadas (usadas quando o arquivo nao tem vn)
    std::vector<float> VN(V.size(), 0.0f);
    for (size_t i = 0; i < tris.size(); ++i) {
        const float* p0 = &V[tris[i].c[0].v * 3];
        const float* p1 = &V[tris[i].c[1].v * 3];
        const float* p2 = &V[tris[i].c[2].v * 3];
        float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        float fn[3];
        cross(e1, e2, fn);  // comprimento proporcional a area
        for (int k = 0; k < 3; ++k)
            for (int a = 0; a < 3; ++a) VN[tris[i].c[k].v * 3 + a] += fn[a];
    }
    for (size_t i = 0; i + 2 < VN.size(); i += 3) {
        if (!normalize(&VN[i])) { VN[i] = 0; VN[i + 1] = 1; VN[i + 2] = 0; }
    }

    // agrupa triangulos por material
    std::vector<std::vector<int> > bucket(materials.size());
    for (size_t i = 0; i < tris.size(); ++i) bucket[tris[i].mat].push_back((int)i);

    for (size_t m = 0; m < materials.size(); ++m) {
        if (bucket[m].empty()) continue;
        ObjPart part;
        part.material = (int)m;
        part.firstVertex = (int)pos.size() / 3;
        part.vertexCount = (int)bucket[m].size() * 3;
        parts.push_back(part);

        for (size_t b = 0; b < bucket[m].size(); ++b) {
            const Tri& t = tris[bucket[m][b]];
            float p[3][3];
            for (int k = 0; k < 3; ++k)
                for (int a = 0; a < 3; ++a) p[k][a] = (V[t.c[k].v * 3 + a] - ctr[a]) * scale;

            float e1[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
            float e2[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
            float fn[3];
            cross(e1, e2, fn);
            bool okFace = normalize(fn);

            for (int k = 0; k < 3; ++k) {
                const Corner& c = t.c[k];
                pos.push_back(p[k][0]); pos.push_back(p[k][1]); pos.push_back(p[k][2]);

                float sn[3] = {VN[c.v * 3], VN[c.v * 3 + 1], VN[c.v * 3 + 2]};
                if (c.n >= 0) {
                    float fnv[3] = {N[c.n * 3], N[c.n * 3 + 1], N[c.n * 3 + 2]};
                    if (normalize(fnv)) { sn[0] = fnv[0]; sn[1] = fnv[1]; sn[2] = fnv[2]; }
                }
                smoothNormals.push_back(sn[0]); smoothNormals.push_back(sn[1]); smoothNormals.push_back(sn[2]);

                if (okFace) { flatNormals.push_back(fn[0]); flatNormals.push_back(fn[1]); flatNormals.push_back(fn[2]); }
                else { flatNormals.push_back(sn[0]); flatNormals.push_back(sn[1]); flatNormals.push_back(sn[2]); }

                if (c.t >= 0) {
                    uv.push_back(T[c.t * 2]); uv.push_back(T[c.t * 2 + 1]);
                } else {
                    // UV automatica (mapeamento esferico em torno do centro)
                    float len = std::sqrt(p[k][0] * p[k][0] + p[k][1] * p[k][1] + p[k][2] * p[k][2]);
                    float u = 0.5f + std::atan2(p[k][2], p[k][0]) / (2.0f * kPi);
                    float v = len > 1e-12f ? 0.5f + std::asin(p[k][1] / len) / kPi : 0.5f;
                    uv.push_back(u); uv.push_back(v);
                    uvGenerated = true;
                }
            }
        }
    }
    return true;
}
