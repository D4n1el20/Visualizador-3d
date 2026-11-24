#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }  // gdiplus.h espera min/max
#include <gdiplus.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "TextureLoader.h"
#include "EnvMap.h"

#pragma comment(lib, "gdiplus.lib")

static ULONG_PTR g_gdiToken = 0;
static bool g_gdiOn = false;

static bool ensureGdi() {
    if (g_gdiOn) return true;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiToken, &input, NULL) != Gdiplus::Ok) return false;
    g_gdiOn = true;
    return true;
}

void shutdownImageLoader() {
    if (g_gdiOn) { Gdiplus::GdiplusShutdown(g_gdiToken); g_gdiOn = false; }
}

bool loadImageRGBA(const std::string& path, int& w, int& h, std::vector<unsigned char>& rgba, std::string& err) {
    if (!ensureGdi()) { err = "falha ao iniciar GDI+"; return false; }

    std::string p = path;
    for (size_t i = 0; i < p.size(); ++i) if (p[i] == '/') p[i] = '\\';
    int n = MultiByteToWideChar(CP_ACP, 0, p.c_str(), -1, NULL, 0);
    if (n <= 0) { err = "caminho invalido"; return false; }
    std::vector<wchar_t> wp(n);
    MultiByteToWideChar(CP_ACP, 0, p.c_str(), -1, &wp[0], n);

    bool ok = false;
    {   // o Bitmap precisa ser destruido antes do GdiplusShutdown
        Gdiplus::Bitmap bmp(&wp[0], FALSE);
        if (bmp.GetLastStatus() != Gdiplus::Ok) {
            err = "nao foi possivel abrir a imagem (formato nao suportado ou arquivo inexistente)";
        } else {
            w = (int)bmp.GetWidth();
            h = (int)bmp.GetHeight();
            Gdiplus::Rect rect(0, 0, w, h);
            Gdiplus::BitmapData bd;
            if (bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
                err = "falha ao ler os pixels da imagem";
            } else {
                rgba.assign((size_t)w * h * 4, 255);
                for (int y = 0; y < h; ++y) {
                    const unsigned char* src = (const unsigned char*)bd.Scan0 + (ptrdiff_t)y * bd.Stride;
                    unsigned char* dst = &rgba[(size_t)(h - 1 - y) * w * 4];  // inverte: GDI+ comeca pelo topo
                    for (int x = 0; x < w; ++x) {
                        dst[4 * x + 0] = src[4 * x + 2];  // R
                        dst[4 * x + 1] = src[4 * x + 1];  // G
                        dst[4 * x + 2] = src[4 * x + 0];  // B
                        dst[4 * x + 3] = src[4 * x + 3];  // A
                    }
                }
                bmp.UnlockBits(&bd);
                ok = true;
            }
        }
    }
    return ok;
}

static unsigned int uploadRGBA(int w, int h, const unsigned char* data, bool clampEdges) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampEdges ? GL_CLAMP : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampEdges ? GL_CLAMP : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return id;
}

unsigned int createTextureFromFile(const std::string& path, std::string& err) {
    int w = 0, h = 0;
    std::vector<unsigned char> px;
    if (!loadImageRGBA(path, w, h, px, err)) return 0;
    return uploadRGBA(w, h, &px[0], false);
}

unsigned int createEnvironmentTexture() {
    std::vector<unsigned char> px;
    generateSphereMapRGBA(256, px);
    return uploadRGBA(256, 256, &px[0], true);
}

void deleteTexture(unsigned int id) {
    if (id) { GLuint g = id; glDeleteTextures(1, &g); }
}
