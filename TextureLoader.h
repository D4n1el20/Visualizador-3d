// Carregamento de imagens (PNG/BMP/JPG) via GDI+ do Windows e criacao de texturas OpenGL.
#ifndef TEXTURELOADER_H
#define TEXTURELOADER_H
#include <string>
#include <vector>

// Le a imagem para RGBA 8 bits, com a linha 0 = base da imagem (convencao do OpenGL / UV do OBJ).
bool loadImageRGBA(const std::string& path, int& w, int& h, std::vector<unsigned char>& rgba, std::string& err);
// Cria textura 2D com mipmaps. Retorna 0 em caso de falha. Requer contexto OpenGL ativo.
unsigned int createTextureFromFile(const std::string& path, std::string& err);
// Textura Sphere Map procedural para a reflexao de ambiente.
unsigned int createEnvironmentTexture();
void deleteTexture(unsigned int id);
void shutdownImageLoader();
#endif
