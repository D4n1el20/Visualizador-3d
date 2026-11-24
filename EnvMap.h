// Gera proceduralmente uma imagem de Sphere Map (ceu, chao, faixas e luzes).
#ifndef ENVMAP_H
#define ENVMAP_H
#include <vector>
// out recebe size*size pixels RGBA (8 bits). Linha 0 = t = 0 (convencao do OpenGL).
void generateSphereMapRGBA(int size, std::vector<unsigned char>& out);
#endif
