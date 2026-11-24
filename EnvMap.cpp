#include "EnvMap.h"
#include <cmath>

namespace {
float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
float mixf(float a, float b, float t) { return a + (b - a) * t; }
}

void generateSphereMapRGBA(int n, std::vector<unsigned char>& out) {
    out.assign((size_t)n * n * 4, 255);

    // direcoes das luzes (normalizadas)
    const float l1[3] = {0.5f / 1.0112f, 0.75f / 1.0112f, 0.45f / 1.0112f};   // |(0.5,0.75,0.45)| = 1.0112
    const float l2[3] = {-0.7f / 0.9220f, 0.3f / 0.9220f, 0.5f / 0.9220f};     // |(-0.7,0.3,0.5)| = 0.9220

    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            // (s,t) do sphere map -> normal da esfera -> vetor refletido R (visao em +Z)
            float nx = 2.0f * (x + 0.5f) / n - 1.0f;
            float ny = 2.0f * (y + 0.5f) / n - 1.0f;
            float r2 = nx * nx + ny * ny;
            if (r2 > 1.0f) { float r = std::sqrt(r2); nx /= r; ny /= r; r2 = 1.0f; }
            float nz = std::sqrt(1.0f - r2);
            float Rx = 2.0f * nz * nx, Ry = 2.0f * nz * ny, Rz = 2.0f * nz * nz - 1.0f;

            float c[3];
            if (Ry >= 0) {  // ceu
                float t = std::pow(clamp01(Ry), 0.5f);
                c[0] = mixf(0.80f, 0.20f, t); c[1] = mixf(0.86f, 0.40f, t); c[2] = mixf(0.92f, 0.80f, t);
            } else {        // chao
                float t = std::pow(clamp01(-Ry), 0.6f);
                c[0] = mixf(0.55f, 0.10f, t); c[1] = mixf(0.52f, 0.09f, t); c[2] = mixf(0.48f, 0.08f, t);
            }
            // faixas verticais perto do horizonte (ajudam a ver o reflexo girando)
            float az = std::atan2(Rx, Rz);
            float w = 1.0f - clamp01(std::fabs(Ry) * 3.0f);
            float band = 0.5f + 0.5f * std::sin(5.0f * az);
            for (int k = 0; k < 3; ++k) c[k] *= 1.0f - 0.45f * w * band;

            // luzes
            float d1 = Rx * l1[0] + Ry * l1[1] + Rz * l1[2]; if (d1 < 0) d1 = 0;
            float d2 = Rx * l2[0] + Ry * l2[1] + Rz * l2[2]; if (d2 < 0) d2 = 0;
            float s1 = 1.8f * std::pow(d1, 80.0f) + 0.25f * std::pow(d1, 8.0f);
            float s2 = 0.9f * std::pow(d2, 40.0f);
            c[0] += 1.00f * s1 + 0.60f * s2;
            c[1] += 0.95f * s1 + 0.75f * s2;
            c[2] += 0.85f * s1 + 1.00f * s2;

            unsigned char* px = &out[((size_t)y * n + x) * 4];
            for (int k = 0; k < 3; ++k) px[k] = (unsigned char)(clamp01(c[k]) * 255.0f + 0.5f);
            px[3] = 255;
        }
    }
}
