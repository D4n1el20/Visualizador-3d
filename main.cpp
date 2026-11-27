// Trabalho 2 - Computacao Grafica: visualizador 3D (C++ / OpenGL / GLUT / GLUI)
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <GL/glut.h>
#include <GL/glui.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "ObjModel.h"
#include "TextureLoader.h"

#pragma comment(lib, "comdlg32.lib")

// Constantes do OpenGL 1.2 que o gl.h do Windows (1.1) nao declara
#ifndef GL_LIGHT_MODEL_COLOR_CONTROL
#define GL_LIGHT_MODEL_COLOR_CONTROL 0x81F8
#endif
#ifndef GL_SEPARATE_SPECULAR_COLOR
#define GL_SEPARATE_SPECULAR_COLOR 0x81FA
#endif

// ---------------------------------------------------------------- estado
static int mainWin = 0;
static GLUI* glui = NULL;
static GLUI_StaticText* stInfo = NULL;
static GLUI_StaticText* stUV = NULL;
static GLUI_StaticText* stTex = NULL;
static GLUI_StaticText* stStatus = NULL;

static ObjModel model;
static unsigned int customTex = 0;   // textura escolhida pelo usuario (0 = usar a do .mtl)
static unsigned int envTex = 0;      // sphere map procedural

// controles (variaveis "vivas" do GLUI)
static int   lightingOn = 1;
static float lightPos[3] = { 3.0f, 4.0f, 5.0f };
static int   shadeMode = 1;          // 0 = Flat, 1 = Smooth (Gouraud)
static int   opacityPct = 100;       // 0..100
static int   seeInterior = 0;        // transparencia mostrando as faces de tras
static int   shininess = 40;         // 0..128
static int   specPct = 80;           // intensidade do brilho especular
static int   surfaceMode = 0;        // 0 = textura, 1 = monocromatico, 2 = so reflexao
static int   envOverlay = 0;         // soma reflexao de ambiente nos modos 0 e 1
static int   envPct = 40;
static float monoColor[3] = { 0.75f, 0.75f, 0.80f };
static float rotX = 15.0f, rotY = -30.0f, camDist = 4.5f;

static int lastX = 0, lastY = 0, dragButton = -1;

enum { ID_REDRAW = 1, ID_LOADOBJ, ID_LOADTEX, ID_ORIGTEX, ID_RESETCAM, ID_RESETLIGHT, ID_QUIT };

// ---------------------------------------------------------------- utilitarios
static void redraw() {
    glutSetWindow(mainWin);
    glutPostRedisplay();
}

static std::string baseName(const std::string& p) {
    size_t i = p.find_last_of("/\\");
    return i == std::string::npos ? p : p.substr(i + 1);
}

static void setStatus(const std::string& s) {
    if (stStatus) stStatus->set_text(s.c_str());
}

static bool fileExists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// Janela padrao do Windows para escolher arquivo
static bool openFileDialog(const char* filter, const char* title, std::string& out) {
    char buf[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return false;
    out = buf;
    return true;
}

static void updateInfoText() {
    char b[128];
    sprintf(b, "Triangulos: %d  Materiais: %d", model.triangleCount(), (int)model.parts.size());
    if (stInfo) stInfo->set_text(b);
    if (stUV) stUV->set_text(model.uvGenerated ? "UV: geradas automaticamente"
        : (model.hasFileUV ? "UV: do arquivo (vt)" : "UV: -"));
    if (stTex) {
        std::string t = "Textura: ";
        if (customTex) t += "(escolhida pelo usuario)";
        else if (!model.materials.empty() && model.materials[0].texId) t += baseName(model.materials[0].mapKdPath);
        else t += "nenhuma";
        stTex->set_text(t.c_str());
    }
}

static void freeModelTextures() {
    for (size_t i = 0; i < model.materials.size(); ++i) {
        deleteTexture(model.materials[i].texId);
        model.materials[i].texId = 0;
    }
}

static bool loadModelFile(const std::string& path) {
    glutSetWindow(mainWin);  // contexto OpenGL da janela principal
    ObjModel m;
    std::string err;
    if (!m.load(path, err)) {
        setStatus("Erro ao abrir OBJ: " + err);
        return false;
    }
    freeModelTextures();
    model = m;
    int missing = 0;
    for (size_t i = 0; i < model.materials.size(); ++i) {
        if (model.materials[i].mapKdPath.empty()) continue;
        std::string e;
        model.materials[i].texId = createTextureFromFile(model.materials[i].mapKdPath, e);
        if (!model.materials[i].texId) ++missing;
    }
    updateInfoText();
    setStatus(missing ? "OBJ carregado (textura nao encontrada)" : "OBJ carregado: " + baseName(path));
    return true;
}

static void loadCustomTexture(const std::string& path) {
    glutSetWindow(mainWin);
    std::string err;
    unsigned int t = createTextureFromFile(path, err);
    if (!t) { setStatus("Erro na textura: " + err); return; }
    deleteTexture(customTex);
    customTex = t;
    updateInfoText();
    setStatus("Textura carregada: " + baseName(path));
}

// ---------------------------------------------------------------- desenho
static void bindArrays(bool useUV) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, &model.pos[0]);
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, shadeMode == 0 ? &model.flatNormals[0] : &model.smoothNormals[0]);
    if (useUV) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, &model.uv[0]);
    }
    else {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
}

static void applyLighting() {
    if (lightingOn) { glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); }
    else glDisable(GL_LIGHTING);
}

static void setEnvTexGen(bool on) {
    if (on) {
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
        glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
        glEnable(GL_TEXTURE_GEN_S);
        glEnable(GL_TEXTURE_GEN_T);
    }
    else {
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
    }
}

// Passo principal: textura, cor unica ou apenas reflexao
static void drawBasePass(float alpha) {
    if (model.pos.empty()) return;
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    if (surfaceMode == 2) {                         // reflexao de ambiente (Sphere Map)
        bindArrays(false);
        setEnvTexGen(true);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, envTex);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glDrawArrays(GL_TRIANGLES, 0, model.vertexCount());
        setEnvTexGen(false);
    }
    else if (surfaceMode == 1) {                  // monocromatico
        bindArrays(false);
        glDisable(GL_TEXTURE_2D);
        glColor4f(monoColor[0], monoColor[1], monoColor[2], alpha);
        glDrawArrays(GL_TRIANGLES, 0, model.vertexCount());
    }
    else {                                        // texturizado
        bindArrays(true);
        for (size_t i = 0; i < model.parts.size(); ++i) {
            const ObjPart& p = model.parts[i];
            const ObjMaterial& m = model.materials[p.material];
            unsigned int tex = customTex ? customTex : m.texId;
            if (tex) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, tex);
                glColor4f(1.0f, 1.0f, 1.0f, alpha);
            }
            else {
                glDisable(GL_TEXTURE_2D);
                glColor4f(m.kd[0], m.kd[1], m.kd[2], alpha);
            }
            glDrawArrays(GL_TRIANGLES, p.firstVertex, p.vertexCount);
        }
    }
    glDisable(GL_TEXTURE_2D);
}

// Segundo passo: soma a reflexao de ambiente por cima da superficie
static void drawEnvOverlay(float alpha) {
    if (model.pos.empty()) return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    bindArrays(false);
    setEnvTexGen(true);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, envTex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1.0f, 1.0f, 1.0f, (envPct / 100.0f) * alpha);
    glDrawArrays(GL_TRIANGLES, 0, model.vertexCount());
    setEnvTexGen(false);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    applyLighting();
}

static void drawPasses(float alpha) {
    drawBasePass(alpha);
    if (envOverlay && surfaceMode != 2) drawEnvOverlay(alpha);
}

static void display() {
    glClearColor(0.12f, 0.13f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_NORMALIZE);
    glShadeModel(shadeMode == 0 ? GL_FLAT : GL_SMOOTH);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, camDist, 0, 0, 0, 0, 1, 0);

    // luz fixa no mundo (o modelo e que gira)
    float lp[4] = { lightPos[0], lightPos[1], lightPos[2], 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    applyLighting();

    // material especular (modelo de Phong)
    float spec = specPct / 100.0f;
    float ms[4] = { spec, spec, spec, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, ms);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, (float)shininess);

    // marcador da luz (opaco, sem iluminacao)
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3f(1.0f, 0.9f, 0.2f);
    glPushMatrix();
    glTranslatef(lightPos[0], lightPos[1], lightPos[2]);
    glutSolidSphere(0.08, 12, 12);
    glPopMatrix();
    applyLighting();

    // modelo
    glPushMatrix();
    glRotatef(rotX, 1, 0, 0);
    glRotatef(rotY, 0, 1, 0);

    float a = opacityPct / 100.0f;
    if (a >= 0.999f) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        drawPasses(1.0f);
    }
    else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (!seeInterior && !model.pos.empty()) {
            // 1) preenche so o depth buffer; 2) desenha a cor onde a profundidade coincide:
            //    sobra apenas a camada mais proxima, sem faces misturadas em ordem errada
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDepthMask(GL_TRUE);
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            bindArrays(false);
            glDrawArrays(GL_TRIANGLES, 0, model.vertexCount());
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            applyLighting();
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
            drawPasses(a);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
        }
        else {
            // transparencia completa: faces de tras primeiro, depois as da frente;
            // nao escreve no depth buffer, mas ainda testa contra o que ja foi desenhado
            glDepthMask(GL_FALSE);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            drawPasses(a);
            glCullFace(GL_BACK);
            drawPasses(a);
            glDisable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
        }
        glDisable(GL_BLEND);
    }
    glPopMatrix();

    glutSwapBuffers();
}

static void reshape(int, int) {
    int tx, ty, tw, th;
    GLUI_Master.get_viewport_area(&tx, &ty, &tw, &th);  // area livre ao lado do painel GLUI
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    glViewport(tx, ty, tw, th);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)tw / (double)th, 0.1, 100.0);  // aspect ratio preservado
    glMatrixMode(GL_MODELVIEW);
    glutPostRedisplay();
}

// ---------------------------------------------------------------- entrada
static void keyboard(unsigned char key, int, int) {
    if (key == 27) exit(0);
}

static void clampCam() {
    if (camDist < 1.5f) camDist = 1.5f;
    if (camDist > 20.0f) camDist = 20.0f;
}

static void mouse(int button, int state, int x, int y) {
    if (button == 3 || button == 4) {  // roda do mouse (freeglut)
        if (state == GLUT_DOWN) {
            camDist += (button == 3) ? -0.3f : 0.3f;
            clampCam();
            if (glui) glui->sync_live();
            glutPostRedisplay();
        }
        return;
    }
    if (state == GLUT_DOWN) { dragButton = button; lastX = x; lastY = y; }
    else dragButton = -1;
}

static void motion(int x, int y) {
    if (dragButton == GLUT_LEFT_BUTTON) {
        rotY += (x - lastX) * 0.5f;
        rotX += (y - lastY) * 0.5f;
    }
    else if (dragButton == GLUT_RIGHT_BUTTON) {
        camDist += (y - lastY) * 0.03f;
        clampCam();
        if (glui) glui->sync_live();
    }
    lastX = x;
    lastY = y;
    glutPostRedisplay();
}

static void onControl(int id) {
    switch (id) {
    case ID_LOADOBJ: {
        std::string f;
        if (openFileDialog("Arquivos OBJ (*.obj)\0*.obj\0Todos os arquivos\0*.*\0", "Escolha um modelo OBJ", f))
            loadModelFile(f);
        break;
    }
    case ID_LOADTEX: {
        std::string f;
        if (openFileDialog("Imagens (*.png;*.bmp;*.jpg;*.jpeg)\0*.png;*.bmp;*.jpg;*.jpeg\0Todos os arquivos\0*.*\0",
            "Escolha uma textura", f))
            loadCustomTexture(f);
        break;
    }
    case ID_ORIGTEX:
        glutSetWindow(mainWin);
        deleteTexture(customTex);
        customTex = 0;
        updateInfoText();
        setStatus("Usando a textura do modelo");
        break;
    case ID_RESETCAM:
        rotX = 15.0f; rotY = -30.0f; camDist = 4.5f;
        if (glui) glui->sync_live();
        break;
    case ID_RESETLIGHT:
        lightingOn = 1;
        lightPos[0] = 3.0f; lightPos[1] = 4.0f; lightPos[2] = 5.0f;
        shadeMode = 1;
        if (glui) glui->sync_live();
        setStatus("Iluminacao restaurada");
        break;
    case ID_QUIT:
        exit(0);
        break;
    default:
        break;
    }
    redraw();
}

// Slider horizontal (inteiro) do GLUI
static GLUI_Scrollbar* addSlider(GLUI_Panel* parent, const char* name, int* var, int lo, int hi) {
    GLUI_Scrollbar* sb = new GLUI_Scrollbar(parent, name, GLUI_SCROLL_HORIZONTAL, var, ID_REDRAW, onControl);
    sb->set_int_limits(lo, hi);
    return sb;
}

// ---------------------------------------------------------------- interface
static void buildInterface() {
    glui = GLUI_Master.create_glui_subwindow(mainWin, GLUI_SUBWINDOW_RIGHT);

    GLUI_Panel* pModel = glui->add_panel("Modelo");
    stInfo = glui->add_statictext_to_panel(pModel, "Triangulos: -");
    stUV = glui->add_statictext_to_panel(pModel, "UV: -");
    glui->add_button_to_panel(pModel, "Carregar OBJ...", ID_LOADOBJ, onControl);

    GLUI_Rollout* rLight = glui->add_rollout("Iluminacao e sombreamento", true);
    glui->add_checkbox_to_panel(rLight, "Luz ligada", &lightingOn, ID_REDRAW, onControl);
    const char* axis[3] = { "Luz X", "Luz Y", "Luz Z" };
    for (int i = 0; i < 3; ++i) {
        GLUI_Spinner* sp = glui->add_spinner_to_panel(rLight, axis[i], GLUI_SPINNER_FLOAT, &lightPos[i], ID_REDRAW, onControl);
        sp->set_float_limits(-10.0f, 10.0f);
        sp->set_speed(0.5f);
    }
    GLUI_Panel* pShade = glui->add_panel_to_panel(rLight, "Sombreamento");
    GLUI_RadioGroup* rgShade = glui->add_radiogroup_to_panel(pShade, &shadeMode, ID_REDRAW, onControl);
    glui->add_radiobutton_to_group(rgShade, "Flat");
    glui->add_radiobutton_to_group(rgShade, "Smooth (Gouraud)");
    glui->add_button_to_panel(rLight, "Resetar iluminacao", ID_RESETLIGHT, onControl);

    GLUI_Rollout* rSpec = glui->add_rollout("Reflexao especular (Phong)", true);
    glui->add_statictext_to_panel(rSpec, "Brilho (shininess 0-128)");
    addSlider(rSpec, "sbShin", &shininess, 0, 128);
    glui->add_statictext_to_panel(rSpec, "Intensidade do brilho (%)");
    addSlider(rSpec, "sbSpec", &specPct, 0, 100);

    GLUI_Rollout* rTransp = glui->add_rollout("Transparencia", true);
    glui->add_statictext_to_panel(rTransp, "Opacidade (%)");
    addSlider(rTransp, "sbOp", &opacityPct, 0, 100);
    glui->add_checkbox_to_panel(rTransp, "Ver faces internas", &seeInterior, ID_REDRAW, onControl);

    GLUI_Rollout* rSurf = glui->add_rollout("Superficie e texturas", true);
    GLUI_RadioGroup* rgSurf = glui->add_radiogroup_to_panel(rSurf, &surfaceMode, ID_REDRAW, onControl);
    glui->add_radiobutton_to_group(rgSurf, "Texturizado");
    glui->add_radiobutton_to_group(rgSurf, "Monocromatico");
    glui->add_radiobutton_to_group(rgSurf, "Reflexao (Sphere Map)");
    const char* cn[3] = { "Cor R", "Cor G", "Cor B" };
    for (int i = 0; i < 3; ++i) {
        GLUI_Spinner* sp = glui->add_spinner_to_panel(rSurf, cn[i], GLUI_SPINNER_FLOAT, &monoColor[i], ID_REDRAW, onControl);
        sp->set_float_limits(0.0f, 1.0f);
        sp->set_speed(0.5f);
    }
    glui->add_checkbox_to_panel(rSurf, "Somar reflexao de ambiente", &envOverlay, ID_REDRAW, onControl);
    glui->add_statictext_to_panel(rSurf, "Forca da reflexao (%)");
    addSlider(rSurf, "sbEnv", &envPct, 0, 100);
    stTex = glui->add_statictext_to_panel(rSurf, "Textura: -");
    glui->add_button_to_panel(rSurf, "Carregar textura...", ID_LOADTEX, onControl);
    glui->add_button_to_panel(rSurf, "Textura original", ID_ORIGTEX, onControl);

    GLUI_Panel* pCam = glui->add_panel("Camera (mouse: esq. gira, dir. zoom)");
    GLUI_Spinner* spD = glui->add_spinner_to_panel(pCam, "Distancia", GLUI_SPINNER_FLOAT, &camDist, ID_REDRAW, onControl);
    spD->set_float_limits(1.5f, 20.0f);
    glui->add_button_to_panel(pCam, "Resetar camera", ID_RESETCAM, onControl);

    stStatus = glui->add_statictext("Pronto");
    glui->add_button("Sair", ID_QUIT, onControl);
}

// Procura o modelo padrao em locais relativos (funciona rodando pelo VS ou pelo .exe)
static std::string findDefaultModel() {
    char exe[MAX_PATH];
    std::string exeDir;
    if (GetModuleFileNameA(NULL, exe, MAX_PATH)) {
        exeDir = exe;
        size_t i = exeDir.find_last_of("\\/");
        exeDir = (i == std::string::npos) ? std::string() : exeDir.substr(0, i + 1);
    }
    const char* rel = "models/bb8/TFA_BB-8_LOD_0.obj";
    const char* prefix[] = { "", "../", "../../", "../../../" };
    for (int i = 0; i < 4; ++i) {
        std::string a = std::string(prefix[i]) + rel;
        if (fileExists(a)) return a;
        std::string b = exeDir + prefix[i] + rel;
        if (fileExists(b)) return b;
    }
    return "";
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(960, 720);  // area 3D (sem o painel) fica acima de 600x600
    glutInitWindowPosition(80, 60);
    mainWin = glutCreateWindow("Trabalho 2 - Visualizador 3D (BB-8)");

    glutDisplayFunc(display);
    GLUI_Master.set_glutReshapeFunc(reshape);
    GLUI_Master.set_glutKeyboardFunc(keyboard);
    GLUI_Master.set_glutMouseFunc(mouse);
    glutMotionFunc(motion);
    GLUI_Master.set_glutIdleFunc(NULL);  // redesenha so quando algo muda

    // estado fixo do OpenGL
    float amb[4] = { 0.18f, 0.18f, 0.18f, 1.0f };
    float la[4] = { 0.15f, 0.15f, 0.15f, 1.0f }, ld[4] = { 1, 1, 1, 1 }, ls[4] = { 1, 1, 1, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);                              // faces de tras tambem iluminadas
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);      // brilho nao e apagado pela textura
    glLightfv(GL_LIGHT0, GL_AMBIENT, la);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ld);
    glLightfv(GL_LIGHT0, GL_SPECULAR, ls);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    envTex = createEnvironmentTexture();
    buildInterface();

    std::string path = (argc > 1) ? std::string(argv[1]) : findDefaultModel();
    if (!path.empty()) loadModelFile(path);
    else setStatus("Modelo padrao nao encontrado: use Carregar OBJ");
    updateInfoText();

    atexit(shutdownImageLoader);
    glutMainLoop();
    return 0;
}