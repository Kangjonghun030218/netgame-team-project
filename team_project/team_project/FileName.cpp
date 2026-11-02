#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- 구조체 정의 ---

struct PlanetData {
    std::string name;
    float distance;
    float size;
    float orbitSpeed;
    float selfRotateSpeed;
    float axisTilt;
    GLuint textureID;

    int health;
    float initialSize;
    bool active;

    // 리얼리티 정보
    std::string realDiameter;
    std::string realDay;
    std::string realYear;
    std::string planetType;
};

bool operator==(const PlanetData& a, const PlanetData& b) {
    return a.name == b.name;
}

struct AsteroidData {
    glm::vec3 position;
    glm::vec3 velocity;
    float size;
    float initialSize;
    float rotationSpeed;
    glm::vec3 rotationAxis;
    int health;
};

struct MissileData {
    glm::vec3 position;
    glm::vec3 velocity;
    // 각도 변수 제거 (그릴 때 velocity로 계산)
    float lifeTime;
    bool active;
};

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float life;
    float size;
    float fadeSpeed;
};

// --- 전역 변수 ---
GLint width = 1200, height = 800;
GLuint shaderProgramID;
GLuint vertexShader, fragmentShader;
GLuint modelLoc, viewLoc, projLoc, colorLoc, lightPosLoc, viewPosLoc, isSunLoc;
GLuint whiteTex;

// VAO/VBO
GLuint sphereVAO, sphereVBO, sphereEBO;
GLuint asteroidVAO, asteroidVBO;
GLuint starVAO, starVBO;
GLuint ringVAO, ringVBO;
GLuint saturnRingTex;

size_t sphereIndexCount = 0;

// 텍스처 ID
GLuint sunTex, mercuryTex, venusTex, earthTex, moonTex, marsTex, jupiterTex, saturnTex, uranusTex, neptuneTex;
GLuint backgroundTex;

// 데이터 컨테이너
std::vector<PlanetData> planets;
std::vector<AsteroidData> asteroids;
std::vector<MissileData> missiles;
std::vector<Particle> particles;

// 게임 설정 상수
const int asteroidCount = 1500;
const int starCount = 8000;
const float missileSpeed = 4.0f;
const float missileRange = 5000.0f;
const float PI = 3.1415926535f;

// 우주선(플레이어)
glm::vec3 shipPos(0.0f, 0.0f, 1500.0f);
float shipAngle = 180.0f;
float shipPitch = 0.0f;
bool keyState[256] = { false };

// 마우스 제어
float mouseSensitivity = 0.1f;
float lastMouseX = -1.0f, lastMouseY = -1.0f;
bool firstMouse = true;
bool isRightButtonDown = false;
int cameraMode = 1; // 시작 시 1인칭
int targetPlanetIndex = -1;
float mouse_x = 0.0f, mouse_y = 0.0f;

// 시간 및 상태 변수
float globalTime = 0.0f;
bool isRotating = true;
bool isOrbiting = true;
float rotateTime = 0.0f;
float orbitTime = 0.0f;

// UI 관련
bool isTyping = false;
std::string inputString = "";

// ★★★ [엔딩 연출 변수] ★★★
bool isEndingSequence = false;
float endingTime = 0.0f;
bool canExitGame = false;

const int MISSILE_FIRE_DELAY = 10;
int missileFireCounter = 0;

bool isLeftButtonDown = false;

// --- 함수 선언 ---
char* filetobuf(const char* file);
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
GLuint loadTexture(char const* path);

void initSphereData();
void initAsteroidModel();
void initStars();
void initPlanetSettings();
void initAsteroids();
void initRing();
GLuint createWhiteTexture();

void spawnExplosion(glm::vec3 pos, glm::vec3 color, int count);
void spawnTrail(glm::vec3 pos, glm::vec3 color);

void updateParticles();
void updateGameLogic();
void updateMovement();
void fireMissile();

void drawScene();
void drawInputBox();
void drawCrosshair();
void drawPlanetInfo();
void renderBitmapString(float x, float y, void* font, const char* string);

void Reshape(int w, int h);
void Mouse(int button, int state, int x, int y);
void Motion(int x, int y);
void Keyboard(unsigned char key, int x, int y);
void KeyboardUp(unsigned char key, int x, int y);
void Timer(int value);


// --- 구현 ---

char* filetobuf(const char* file) {
    FILE* fptr;
    long length;
    char* buf;
    fptr = fopen(file, "rb");
    if (!fptr) return NULL;
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0;
    return buf;
}

GLuint createWhiteTexture() {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    unsigned char whiteData[] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whiteData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return textureID;
}

void renderBitmapString(float x, float y, void* font, const char* string) {
    const char* c;
    glRasterPos2f(x, y);
    for (c = string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

void drawInputBox() {
    if (!isTyping) return;
    if (isEndingSequence) return; // 엔딩 중에는 입력창 숨김

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glUseProgram(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float boxW = 300.0f;
    float boxH = 50.0f;
    glBegin(GL_QUADS);
    glVertex2f(cx - boxW, cy - boxH);
    glVertex2f(cx + boxW, cy - boxH);
    glVertex2f(cx + boxW, cy + boxH);
    glVertex2f(cx - boxW, cy + boxH);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(1.0f, 1.0f, 1.0f);
    std::string title = "Enter Planet Name:";
    renderBitmapString(cx - 50, cy + 15, GLUT_BITMAP_HELVETICA_18, title.c_str());
    std::string displayStr = inputString + "_";
    renderBitmapString(cx - 30, cy - 15, GLUT_BITMAP_HELVETICA_18, displayStr.c_str());

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glUseProgram(shaderProgramID);
}

GLuint loadTexture(char const* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }
    else {
        std::cout << "Texture load failed: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
}

void initSphereData() {
    std::vector<float> data;
    std::vector<unsigned int> indices;
    const int stackCount = 40;
    const int sectorCount = 40;
    float radius = 1.0f;
    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI / 2 - i * PI / stackCount;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);
        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * 2 * PI / sectorCount;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            data.push_back(x); data.push_back(y); data.push_back(z);
            data.push_back(x / radius); data.push_back(y / radius); data.push_back(z / radius);
            data.push_back((float)j / sectorCount);
            data.push_back((float)i / stackCount);
        }
    }
    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;
        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) { indices.push_back(k1); indices.push_back(k2); indices.push_back(k1 + 1); }
            if (i != (stackCount - 1)) { indices.push_back(k1 + 1); indices.push_back(k2); indices.push_back(k2 + 1); }
        }
    }
    sphereIndexCount = indices.size();
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

void initAsteroidModel() {
    std::vector<float> data;
    const int stackCount = 20;
    const int sectorCount = 20;
    float radius = 1.0f;
    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI / 2 - i * PI / stackCount;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);
        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * 2 * PI / sectorCount;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            float noise = 0.8f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.4f));
            x *= noise; y *= noise; z *= noise;
            data.push_back(x); data.push_back(y); data.push_back(z);
            glm::vec3 n = glm::normalize(glm::vec3(x, y, z));
            data.push_back(n.x); data.push_back(n.y); data.push_back(n.z);
            data.push_back((float)j / sectorCount);
            data.push_back((float)i / stackCount);
        }
    }
    glGenVertexArrays(1, &asteroidVAO);
    glGenBuffers(1, &asteroidVBO);
    glBindVertexArray(asteroidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, asteroidVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

void initStars() {
    std::vector<float> stars;
    for (int i = 0; i < starCount; ++i) {
        // 넓은 범위에 별 뿌리기
        float x = (rand() % 6000) - 3000.0f;
        float y = (rand() % 6000) - 3000.0f;
        float z = (rand() % 6000) - 3000.0f;
        stars.push_back(x); stars.push_back(y); stars.push_back(z);
        stars.push_back(0.0f); stars.push_back(1.0f); stars.push_back(0.0f);
        stars.push_back(0.0f); stars.push_back(0.0f);
    }
    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(float), &stars[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void initPlanetSettings() {
    sunTex = loadTexture("solar.jpg");
    mercuryTex = loadTexture("mercury.jpg");
    venusTex = loadTexture("venus.jpg");
    earthTex = loadTexture("earth.jpg");
    moonTex = loadTexture("moon.jpg");
    marsTex = loadTexture("mars.jpg");
    jupiterTex = loadTexture("jupiter.jpg");
    saturnTex = loadTexture("saturn.jpg");
    uranusTex = loadTexture("uranus.jpg");
    neptuneTex = loadTexture("neptune.jpg");
    backgroundTex = loadTexture("background.jpg");
    saturnRingTex = loadTexture("saturn_ring.png");

    planets.clear();

    // 행성 크기 및 거리 조정
    float baseScale = 1.2f;
    float s_mer = 0.38f * baseScale;
    float s_ven = 0.95f * baseScale;
    float s_ear = 1.0f * baseScale;
    float s_mar = 0.53f * baseScale;
    float s_jup = 11.21f * baseScale;
    float s_sat = 9.45f * baseScale;
    float s_ura = 4.01f * baseScale;
    float s_nep = 3.88f * baseScale;

    // 1. 수성
    planets.push_back({ "mercury", 60.0f, s_mer, 365.0f / 88.0f, 1.0f / 58.65f, 0.03f, mercuryTex, (int)(s_mer * 10.0f), s_mer, true,
        "4,879 km", "58.6 Days", "88 Days", "Terrestrial Planet" });
    // 2. 금성
    planets.push_back({ "venus", 100.0f, s_ven, 365.0f / 226.0f, 1.0f / -243.01f, 177.3f, venusTex, (int)(s_ven * 10.0f), s_ven, true,
        "12,104 km", "243 Days (Retrograde)", "225 Days", "Terrestrial Planet" });
    // 3. 지구
    planets.push_back({ "earth", 140.0f, s_ear, 1.0f, 1.0f, 23.5f, earthTex, (int)(s_ear * 10.0f), s_ear, true,
        "12,742 km", "23.9 Hours", "365.25 Days", "Terrestrial Planet" });
    // 4. 화성
    planets.push_back({ "mars", 180.0f, s_mar, 365.0f / 686.0f, 1.0f / 1.026f, 25.2f, marsTex, (int)(s_mar * 10.0f), s_mar, true,
        "6,779 km", "24.6 Hours", "687 Days", "Terrestrial Planet" });
    // 5. 목성
    planets.push_back({ "jupiter", 300.0f, s_jup, 365.0f / 4343.5f * 5.0f, 1.0f / 0.41f, 3.1f, jupiterTex, (int)(s_jup * 10.0f), s_jup, true,
        "139,820 km", "9.9 Hours", "11.86 Years", "Gas Giant" });
    // 6. 토성
    planets.push_back({ "saturn", 450.0f, s_sat, 365.0f / 10731.0f * 5.0f, 1.0f / 0.426f, 26.7f, saturnTex, (int)(s_sat * 10.0f), s_sat, true,
        "116,460 km", "10.7 Hours", "29.45 Years", "Gas Giant" });
    // 7. 천왕성
    planets.push_back({ "uranus", 600.0f, s_ura, 365.0f / 30587.0f * 10.0f, 1.0f / -0.718f, 97.8f, uranusTex, (int)(s_ura * 10.0f), s_ura, true,
        "50,724 km", "17.2 Hours (Retrograde)", "84 Years", "Ice Giant" });
    // 8. 해왕성
    planets.push_back({ "neptune", 750.0f, s_nep, 365.0f / 59787.0f * 10.0f, 1.0f / 0.67f, 28.3f, neptuneTex, (int)(s_nep * 10.0f), s_nep, true,
        "49,244 km", "16.1 Hours", "164.8 Years", "Ice Giant" });
}

glm::vec3 getPlanetPosition(int index) {
    if (index < 0 || index >= planets.size()) return glm::vec3(0, 0, 0);
    float angle = orbitTime * planets[index].orbitSpeed;
    float rad = glm::radians(angle);
    float x = cos(rad) * planets[index].distance;
    float z = -sin(rad) * planets[index].distance;
    return glm::vec3(x, 0.0f, z);
}

void initAsteroids() {
    asteroids.clear();
    for (int i = 0; i < asteroidCount; ++i) {
        AsteroidData a;
        float range = 950.0f; // 범위 확장
        a.position = glm::vec3(
            (static_cast<float>(rand()) / RAND_MAX * 2.0f * range) - range,
            (static_cast<float>(rand()) / RAND_MAX * 2.0f * range) - range,
            (static_cast<float>(rand()) / RAND_MAX * 2.0f * range) - range
        );
        a.velocity = glm::vec3(
            (static_cast<float>(rand()) / RAND_MAX * 0.1f) - 0.05f,
            (static_cast<float>(rand()) / RAND_MAX * 0.1f) - 0.05f,
            (static_cast<float>(rand()) / RAND_MAX * 0.1f) - 0.05f
        );
        float r = static_cast<float>(rand()) / RAND_MAX;
        a.initialSize = 0.5f + r * 1.0f;
        a.size = a.initialSize;
        a.health = 2 + static_cast<int>(r * 3.0f);
        a.rotationSpeed = (static_cast<float>(rand()) / RAND_MAX * 4.0f) - 2.0f;
        a.rotationAxis = glm::normalize(glm::vec3(rand(), rand(), rand()));
        asteroids.push_back(a);
    }
    initAsteroidModel();
}

void initRing() {
    std::vector<float> data;
    const int segments = 72;
    float innerRadius = 0.6f;
    float outerRadius = 1.2f;
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * 2.0f * PI;
        float c = cosf(angle);
        float s = sinf(angle);
        data.push_back(c * innerRadius); data.push_back(0.0f); data.push_back(s * innerRadius);
        data.push_back(0.0f); data.push_back(1.0f); data.push_back(0.0f);
        data.push_back(0.0f); data.push_back(0.0f);
        data.push_back(c * outerRadius); data.push_back(0.0f); data.push_back(s * outerRadius);
        data.push_back(0.0f); data.push_back(1.0f); data.push_back(0.0f);
        data.push_back(1.0f); data.push_back(0.0f);
    }
    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);
    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

void spawnExplosion(glm::vec3 pos, glm::vec3 color, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = pos;
        p.velocity = glm::normalize(glm::vec3(rand() % 100 - 50, rand() % 100 - 50, rand() % 100 - 50))
            * (0.2f + (rand() % 100 / 100.0f) * 0.3f);
        p.color = color;
        p.life = 1.0f;
        p.size = 0.2f + (rand() % 100 / 100.0f) * 0.3f;
        p.fadeSpeed = 0.02f + (rand() % 100 / 100.0f) * 0.02f;
        particles.push_back(p);
    }
}

void spawnTrail(glm::vec3 pos, glm::vec3 color) {
    Particle p;
    p.position = pos;
    p.velocity = glm::vec3((rand() % 100 - 50) / 400.0f, (rand() % 100 - 50) / 400.0f, (rand() % 100 - 50) / 400.0f);
    p.color = color;
    p.life = 1.0f;
    p.size = 0.3f;
    p.fadeSpeed = 0.05f;
    particles.push_back(p);
}

void updateParticles() {
    for (int i = 0; i < particles.size(); ) {
        particles[i].position += particles[i].velocity;
        particles[i].life -= particles[i].fadeSpeed;
        particles[i].size *= 0.95f;
        if (particles[i].life <= 0.0f) particles.erase(particles.begin() + i);
        else ++i;
    }
}

void updateMovement() {
    // 엔딩 중에는 움직임 불가
    if (isEndingSequence) return;

    glm::vec3 moveDir(0.0f);
    glm::vec3 front;
    front.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    front.y = sin(glm::radians(shipPitch));
    front.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    front = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

    if (keyState['w']) moveDir += front;
    if (keyState['s']) moveDir -= front;
    if (keyState['a']) moveDir -= right;
    if (keyState['d']) moveDir += right;

    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
        float currentSpeed = 0.5f;
        glm::vec3 engineColor(0.2f, 0.6f, 1.0f);
        if (keyState['r']) {
            currentSpeed = 1.5f;
            engineColor = glm::vec3(1.0f, 0.3f, 0.1f);
        }
        shipPos += moveDir * currentSpeed;
        glm::vec3 shipBack = glm::normalize(front) * -1.5f;
        glm::vec3 engineOffset = glm::cross(glm::normalize(shipBack * -1.0f), glm::vec3(0, 1, 0)) * 0.5f;
        spawnTrail(shipPos + shipBack + engineOffset, engineColor);
        spawnTrail(shipPos + shipBack - engineOffset, engineColor);
    }
}

void drawCrosshair() {
    if (cameraMode != 1 && cameraMode != 3) return;
    if (isEndingSequence) return; // 엔딩 중 크로스헤어 숨김

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float size = 15.0f;
    float gap = 5.0f;

    glLineWidth(2.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex2f(cx - size, cy); glVertex2f(cx - gap, cy);
    glVertex2f(cx + gap, cy); glVertex2f(cx + size, cy);
    glVertex2f(cx, cy - size); glVertex2f(cx, cy - gap);
    glVertex2f(cx, cy + gap); glVertex2f(cx, cy + size);
    glEnd();
    glLineWidth(1.0f);

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(shaderProgramID);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void fireMissile() {
    if (isEndingSequence) return;

    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
    glm::vec3 cameraPos, cameraTarget, cameraUp(0, 1, 0);
    glm::vec3 shipDir;
    shipDir.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    shipDir.y = sin(glm::radians(shipPitch));
    shipDir.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    shipDir = glm::normalize(shipDir);

    if (cameraMode == 1) { cameraPos = shipPos + glm::vec3(0, 0.5, 0) + shipDir * 2.0f; cameraTarget = cameraPos + shipDir; }
    else if (cameraMode == 2) { cameraPos = glm::vec3(0, 100, 10); cameraTarget = glm::vec3(0, 0, 0); cameraUp = glm::vec3(0, 0, -1); }
    else { cameraPos = shipPos - (shipDir * 8.0f) + glm::vec3(0, 4, 0); cameraTarget = shipPos + shipDir * 5.0f; }

    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 5000.0f);

    float winX, winY;
    if (cameraMode == 1) {
        winX = (float)viewport[2] / 2.0f;
        winY = (float)viewport[3] / 2.0f;
    }
    else {
        winX = mouse_x;
        winY = (float)viewport[3] - mouse_y;
    }

    glm::vec3 nearPt = glm::unProject(glm::vec3(winX, winY, 0), view, proj, glm::vec4(viewport[0], viewport[1], viewport[2], viewport[3]));
    glm::vec3 farPt = glm::unProject(glm::vec3(winX, winY, 1), view, proj, glm::vec4(viewport[0], viewport[1], viewport[2], viewport[3]));
    glm::vec3 rayOrigin = cameraPos;
    glm::vec3 rayDir = glm::normalize(farPt - nearPt);

    glm::vec3 targetPos;
    bool hit = false;
    float minDist = 10000.0f;

    for (const auto& a : asteroids) {
        if (a.size <= 0) continue;
        glm::vec3 aPos = a.position + (a.velocity * globalTime);
        glm::vec3 L = aPos - rayOrigin;
        float tca = dot(L, rayDir);
        if (tca < 0) continue;
        float d2 = dot(L, L) - tca * tca;
        float r2 = (a.size + 1.0f) * (a.size + 1.0f);
        if (d2 > r2) continue;
        float thc = sqrt(r2 - d2);
        float t0 = tca - thc;
        if (t0 < minDist) { minDist = t0; targetPos = aPos; hit = true; }
    }

    if (!hit) targetPos = rayOrigin + rayDir * 5000.0f;

    MissileData m;
    glm::vec3 fireDir = glm::normalize(targetPos - shipPos);
    m.position = shipPos + fireDir * 2.0f;
    m.velocity = fireDir * missileSpeed;
    m.lifeTime = 0.0f;
    m.active = true;
    missiles.push_back(m);
}

void updateGameLogic() {
    // 미사일 위치 업데이트 및 삭제
    for (int i = 0; i < missiles.size(); ) {
        if (!missiles[i].active) {
            missiles.erase(missiles.begin() + i);
            continue;
        }
        missiles[i].position += missiles[i].velocity;
        if (glm::length(missiles[i].position) > missileRange) missiles.erase(missiles.begin() + i);
        else i++;
    }

    // 미사일 충돌 처리
    for (auto& m : missiles) {
        if (!m.active) continue;

        // 행성 충돌
        for (auto& p : planets) {
            if (p.distance == 0.0f) continue;
            glm::vec3 currentPlanetPos = getPlanetPosition(std::distance(planets.begin(), std::find(planets.begin(), planets.end(), p)));
            float dist = glm::length(m.position - currentPlanetPos);

            if (dist < p.size + 0.5f) {
                m.active = false;
                p.health -= 50; // 데미지
                spawnExplosion(m.position, glm::vec3(0.1f, 0.5f, 0.2f), 5);

                float initialHealth = p.initialSize * 10.0f;
                float hpRatio = (float)p.health / initialHealth;
                if (p.health > 0) {
                    p.size = p.initialSize * (0.5f + 0.5f * hpRatio);
                }
                else {
                    p.size = 0.0f;
                    p.health = 0;
                    spawnExplosion(currentPlanetPos, glm::vec3(1.0f, 0.1f, 0.0f), 100);
                    printf("Planet %s was destroyed!\n", p.name.c_str());
                }
                break;
            }
        }
        // 운석 충돌
        for (auto& a : asteroids) {
            if (a.health <= 0) continue;
            glm::vec3 currentAsteroidPos = a.position + (a.velocity * globalTime);
            float dist = glm::length(m.position - currentAsteroidPos);
            if (dist < a.size + 0.3f) {
                m.active = false;
                a.health -= 50;
                spawnExplosion(currentAsteroidPos, glm::vec3(0.5f, 0.4f, 0.3f), 3);

                float hpRatio = (float)a.health / (float)(a.initialSize * 3.0f);
                if (a.health > 0) a.size = a.initialSize * (0.4f + 0.6f * hpRatio);
                if (a.health <= 0) {
                    a.size = 0.0f;
                    spawnExplosion(currentAsteroidPos, glm::vec3(0.8f, 0.3f, 0.1f), 30);
                }
                break;
            }
        }
    }
    asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(),
        [](const AsteroidData& a) { return a.size <= 0.0f; }), asteroids.end());

    // ★★★ [엔딩 트리거] 모든 행성 파괴 시 엔딩 시작 ★★★
    if (!isEndingSequence) {
        int activePlanetCount = 0;
        for (const auto& p : planets) {
            if (p.distance != 0.0f && p.size > 0.001f) activePlanetCount++;
        }
        if (activePlanetCount == 0) {
            isEndingSequence = true;
            printf("\n\n*** WARNING: SUN STABILITY CRITICAL! ***\n\n");
        }
    }
}

void drawPlanetInfo() {
    if (cameraMode != 4 || targetPlanetIndex == -1) return;
    if (targetPlanetIndex >= planets.size()) return;
    if (isEndingSequence) return;

    PlanetData& p = planets[targetPlanetIndex];

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glUseProgram(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
    float startX = 20.0f;
    float startY = height - 20.0f;
    float boxWidth = 300.0f;
    float boxHeight = 180.0f;
    glBegin(GL_QUADS);
    glVertex2f(startX, startY);
    glVertex2f(startX + boxWidth, startY);
    glVertex2f(startX + boxWidth, startY - boxHeight);
    glVertex2f(startX, startY - boxHeight);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(1.0f, 1.0f, 1.0f);
    char buffer[256];
    float textX = startX + 15.0f;
    float textY = startY - 30.0f;
    float lineGap = 25.0f;

    glColor3f(0.3f, 0.8f, 1.0f);
    std::string nameUpper = p.name;
    std::transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::toupper);
    sprintf(buffer, "=== %s ===", nameUpper.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= (lineGap + 5);

    glColor3f(0.9f, 0.9f, 0.9f);
    sprintf(buffer, "Type: %s", p.planetType.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_12, buffer);
    textY -= lineGap;
    sprintf(buffer, "Real Diameter: %s", p.realDiameter.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= lineGap;
    sprintf(buffer, "Day Length: %s", p.realDay.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= lineGap;
    sprintf(buffer, "Year Length: %s", p.realYear.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= lineGap;
    sprintf(buffer, "Axial Tilt: %.1f deg", p.axisTilt);
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= (lineGap + 5);

    if (p.health > 50) glColor3f(0.2f, 1.0f, 0.2f);
    else if (p.health > 20) glColor3f(1.0f, 0.8f, 0.2f);
    else glColor3f(1.0f, 0.2f, 0.2f);
    sprintf(buffer, "[ Status: HP %d%% ]", (int)((float)p.health / (p.initialSize * 10.0f) * 100.0f));
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(shaderProgramID);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void drawScene() {
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    glm::vec3 cameraPos, cameraTarget, cameraUp(0, 1, 0);
    glm::vec3 shipDir;
    shipDir.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    shipDir.y = sin(glm::radians(shipPitch));
    shipDir.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    shipDir = glm::normalize(shipDir);

    if (cameraMode == 1) {
        cameraPos = shipPos + glm::vec3(0, 0.5, 0) + shipDir * 2.0f;
        cameraTarget = cameraPos + shipDir;
    }
    else if (cameraMode == 2) {
        cameraPos = glm::vec3(0, 1000, 10);
        cameraTarget = glm::vec3(0, 0, 0);
        cameraUp = glm::vec3(0, 0, -1);
    }
    else if (cameraMode == 4 && targetPlanetIndex != -1) {
        glm::vec3 planetPos = getPlanetPosition(targetPlanetIndex);
        float distOffset = planets[targetPlanetIndex].size * 4.0f + 5.0f;
        cameraPos = planetPos + glm::vec3(distOffset, distOffset * 0.5f, distOffset);
        cameraTarget = planetPos;
    }
    else if (cameraMode == 5) { // 전망대 모드
        cameraPos = glm::vec3(0.0f, 150.0f, 0.0f);
        float rotateSpeed = globalTime * 0.2f;
        float viewRadius = 1000.0f;
        cameraTarget.x = cos(glm::radians(rotateSpeed)) * viewRadius;
        cameraTarget.z = sin(glm::radians(rotateSpeed)) * viewRadius;
        cameraTarget.y = -200.0f;
        cameraUp = glm::vec3(0, 1, 0);
    }
    else {
        cameraPos = shipPos - (shipDir * 8.0f) + glm::vec3(0, 4, 0);
        cameraTarget = shipPos + shipDir * 5.0f;
    }
    if (isEndingSequence && endingTime < 6.0f) {
        float shakePower = endingTime * 1.5f; // 시간이 갈수록 더 심하게 흔들림
        cameraPos.x += ((rand() % 100) / 50.0f - 1.0f) * shakePower;
        cameraPos.y += ((rand() % 100) / 50.0f - 1.0f) * shakePower;
        cameraPos.z += ((rand() % 100) / 50.0f - 1.0f) * shakePower;

        // 폭발 순간 타겟도 흔들림
        cameraTarget.x += ((rand() % 100) / 50.0f - 1.0f) * shakePower * 0.5f;
    }

    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 5000.0f);

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(lightPosLoc, 0, 0, 0);
    glUniform3f(viewPosLoc, cameraPos.x, cameraPos.y, cameraPos.z);

    // 배경
    glUniform1i(isSunLoc, 1);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f))));
    glUniform4f(colorLoc, 1, 1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, backgroundTex);
    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    // 별
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    glPointSize(2.0f);
    glBindVertexArray(starVAO);
    glDrawArrays(GL_POINTS, 0, starCount);
    glBindVertexArray(sphereVAO);

    // ★★★ [태양 그리기 - 엔딩 시 거대화] ★★★
    float sunScale = 30.0f;
    if (isEndingSequence) {
        sunScale += (endingTime * endingTime * endingTime) * 10.0f;
    }
    glm::mat4 sunModel = glm::rotate(glm::mat4(1.0f), glm::radians(rotateTime * 0.05f), glm::vec3(0, 1, 0));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::scale(sunModel, glm::vec3(sunScale))));
    glBindTexture(GL_TEXTURE_2D, sunTex);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    // 행성
    for (auto& p : planets) {
        // 궤도선
        glUniform1i(isSunLoc, 1);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniform4f(colorLoc, 0.2f, 0.2f, 0.4f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for (int a = 0; a < 360; a++) glVertex3f(cos(glm::radians((float)a)) * p.distance, 0, sin(glm::radians((float)a)) * p.distance);
        glEnd();

        if (p.size < 0.001f) continue;

        // 행성 본체
        glUniform1i(isSunLoc, 0);
        glm::mat4 pModel = glm::rotate(glm::mat4(1.0f), glm::radians(orbitTime * p.orbitSpeed), glm::vec3(0, 1, 0));
        pModel = glm::translate(pModel, glm::vec3(p.distance, 0, 0));
        glm::mat4 earthPos = pModel;

        pModel = glm::rotate(pModel, glm::radians(p.axisTilt), glm::vec3(0, 0, 1));
        pModel = glm::rotate(pModel, glm::radians(rotateTime * p.selfRotateSpeed * 5.0f), glm::vec3(0, 1, 0));
        pModel = glm::scale(pModel, glm::vec3(p.size));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
        glUniform4f(colorLoc, 1, 1, 1, 1);
        glBindTexture(GL_TEXTURE_2D, p.textureID);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // 지구의 달
        if (p.name == "earth" && p.size > 0.0f) {
            glm::mat4 mModel = glm::rotate(earthPos, glm::radians(orbitTime * 3.0f), glm::vec3(0, 1, 0));
            mModel = glm::translate(mModel, glm::vec3(3.0f, 0, 0));
            mModel = glm::scale(mModel, glm::vec3(0.25f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mModel));
            glBindTexture(GL_TEXTURE_2D, moonTex);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
        // 토성의 고리
        if (p.name == "saturn" && p.size > 0.0f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glUniform1i(isSunLoc, 1);
            glm::mat4 rModel = glm::scale(pModel, glm::vec3(2.0f, 2.0f, 2.0f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(rModel));
            glUniform4f(colorLoc, 1.2f, 1.2f, 1.2f, 0.8f);
            glBindTexture(GL_TEXTURE_2D, saturnRingTex);
            glBindVertexArray(ringVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, (72 + 1) * 2);
            glUniform1i(isSunLoc, 0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glBindVertexArray(sphereVAO);
        }
    }

    // 운석
    glUniform1i(isSunLoc, 0);
    glBindVertexArray(asteroidVAO);
    glBindTexture(GL_TEXTURE_2D, moonTex);
    for (const auto& a : asteroids) {
        if (a.size <= 0.001f) continue;
        glm::mat4 aModel = glm::translate(glm::mat4(1.0f), a.position + a.velocity * globalTime);
        aModel = glm::rotate(aModel, glm::radians(globalTime * a.rotationSpeed), a.rotationAxis);
        aModel = glm::scale(aModel, glm::vec3(a.size));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(aModel));
        float dmgRatio = 1.0f - (a.size / a.initialSize);
        glm::vec3 tint = glm::mix(glm::vec3(1, 1, 1), glm::vec3(1, 0, 0), dmgRatio * 1.5f);
        glUniform4f(colorLoc, tint.r, tint.g, tint.b, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }

    // ★★★ [미사일 그리기 수정] (LookAt 방식 적용) ★★★
    glUniform1i(isSunLoc, 1);
    glBindVertexArray(sphereVAO);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    for (const auto& m : missiles) {
        glm::vec3 dir = glm::normalize(m.velocity);
        // 진행 방향을 바라보는 행렬 생성
        glm::mat4 missileModel = glm::inverse(glm::lookAt(m.position, m.position + dir, glm::vec3(0.0f, 1.0f, 0.0f)));
        // 모델 방향 보정 (180도 회전)
        missileModel = glm::rotate(missileModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 bodyModel = glm::scale(missileModel, glm::vec3(0.1f, 0.1f, 0.8f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bodyModel));
        glUniform4f(colorLoc, 0.2f, 0.8f, 1.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        glm::mat4 headModel = glm::translate(missileModel, glm::vec3(0.0f, 0.0f, 0.8f));
        headModel = glm::scale(headModel, glm::vec3(0.1f, 0.1f, 0.3f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(headModel));
        glUniform4f(colorLoc, 1.0f, 0.2f, 0.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        for (int i = 0; i < 4; ++i) {
            glm::mat4 finModel = glm::rotate(missileModel, glm::radians(i * 90.0f), glm::vec3(0, 0, 1));
            finModel = glm::translate(finModel, glm::vec3(0.15f, 0.0f, -0.6f));
            finModel = glm::scale(finModel, glm::vec3(0.15f, 0.02f, 0.3f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(finModel));
            glUniform4f(colorLoc, 0.6f, 0.6f, 0.6f, 1.0f);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
    }

    // 우주선 그리기
    if (cameraMode != 1) {
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glm::mat4 shipBaseModel = glm::mat4(1.0f);
        shipBaseModel = glm::translate(shipBaseModel, shipPos);
        shipBaseModel = glm::rotate(shipBaseModel, glm::radians(shipAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        shipBaseModel = glm::rotate(shipBaseModel, glm::radians(-shipPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        shipBaseModel = glm::scale(shipBaseModel, glm::vec3(0.5f));

        glm::mat4 bodyModel = glm::scale(shipBaseModel, glm::vec3(0.7f, 0.5f, 2.5f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bodyModel));
        glUniform4f(colorLoc, 0.6f, 0.6f, 0.7f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        glm::mat4 cockpitModel = glm::translate(shipBaseModel, glm::vec3(0.0f, 0.3f, 0.5f));
        cockpitModel = glm::scale(cockpitModel, glm::vec3(0.4f, 0.3f, 0.8f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(cockpitModel));
        glUniform4f(colorLoc, 0.2f, 0.6f, 1.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        glm::mat4 leftWingModel = glm::translate(shipBaseModel, glm::vec3(-1.2f, 0.0f, -0.5f));
        leftWingModel = glm::scale(leftWingModel, glm::vec3(1.5f, 0.1f, 1.2f));
        leftWingModel = glm::rotate(leftWingModel, glm::radians(20.0f), glm::vec3(0, 0, 1));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(leftWingModel));
        glUniform4f(colorLoc, 0.7f, 0.2f, 0.2f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        glm::mat4 rightWingModel = glm::translate(shipBaseModel, glm::vec3(1.2f, 0.0f, -0.5f));
        rightWingModel = glm::scale(rightWingModel, glm::vec3(1.5f, 0.1f, 1.2f));
        rightWingModel = glm::rotate(rightWingModel, glm::radians(-20.0f), glm::vec3(0, 0, 1));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(rightWingModel));
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        for (int i = -1; i <= 1; i += 2) {
            glm::mat4 engineModel = glm::translate(shipBaseModel, glm::vec3(0.6f * i, 0.0f, -1.5f));
            engineModel = glm::scale(engineModel, glm::vec3(0.3f, 0.3f, 0.8f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(engineModel));
            glUniform4f(colorLoc, 0.3f, 0.3f, 0.3f, 1.0f);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

            glm::mat4 nozzleModel = glm::translate(engineModel, glm::vec3(0.0f, 0.0f, -1.0f));
            nozzleModel = glm::scale(nozzleModel, glm::vec3(0.8f, 0.8f, 0.2f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(nozzleModel));
            glUniform4f(colorLoc, 1.0f, 0.5f, 0.0f, 1.0f);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
    }

    // 파티클
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glUniform1i(isSunLoc, 1);
    glBindVertexArray(sphereVAO);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    for (const auto& p : particles) {
        glm::mat4 pModel = glm::scale(glm::translate(glm::mat4(1.0f), p.position), glm::vec3(p.size));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
        glUniform4f(colorLoc, p.color.r, p.color.g, p.color.b, p.life);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ★★★ [엔딩 화면 및 텍스트] ★★★
    if (isEndingSequence) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, width, 0, height);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glDisable(GL_DEPTH_TEST);
        glUseProgram(0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float alpha = 0.0f;
        if (endingTime < 3.0f) {
            alpha = endingTime / 3.0f;
            glColor4f(1.0f, 1.0f, 1.0f, alpha); // White fade
        }
        else {
            alpha = (endingTime - 3.0f) / 2.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            glColor4f(0.0f, 0.0f, 0.0f, alpha); // Black fade
        }

        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(width, 0);
        glVertex2f(width, height);
        glVertex2f(0, height);
        glEnd();

        if (endingTime > 5.0f) {
            canExitGame = true;
            glColor3f(1.0f, 1.0f, 1.0f);
            std::string endText1 = "The Solar System has returned to the Void.";
            std::string endText2 = "PRESS ANY KEY TO EXIT";
            float cx = width / 2.0f;
            float cy = height / 2.0f;
            renderBitmapString(cx - (endText1.length() * 4), cy + 20, GLUT_BITMAP_HELVETICA_18, endText1.c_str());
            float blink = (sin(globalTime * 0.2f) + 1.0f) * 0.5f;
            glColor4f(1.0f, 1.0f, 0.2f, blink);
            renderBitmapString(cx - (endText2.length() * 4), cy - 20, GLUT_BITMAP_HELVETICA_18, endText2.c_str());
        }

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glUseProgram(shaderProgramID);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    drawCrosshair();
    drawPlanetInfo();
    drawInputBox();
    glutSwapBuffers();
}

void Keyboard(unsigned char key, int x, int y) {
    // 엔딩 시 아무 키나 누르면 종료
    if (canExitGame) {
        exit(0);
    }

    if (isTyping) {
        if (key == 13) {
            bool found = false;
            for (int i = 0; i < planets.size(); ++i) {
                if (planets[i].name == inputString) {
                    targetPlanetIndex = i;
                    cameraMode = 4;
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("Planet '%s' not found!\n", inputString.c_str());
            }
            isTyping = false;
            inputString = "";
        }
        else if (key == 8) {
            if (inputString.length() > 0) {
                inputString.pop_back();
            }
        }
        else if (key == 27) {
            isTyping = false;
            inputString = "";
        }
        else if (key >= 32 && key <= 126) {
            inputString += (char)key;
        }
        return;
    }

    if (key == 27) exit(0);

    if (key == 'v' || key == 'V') {
        isTyping = true;
        inputString = "";
        keyState[key] = false;
        return;
    }

    if (key >= 'A' && key <= 'Z') key = key + ('a' - 'A');

    keyState[key] = true;

    if (key == 'y') isRotating = !isRotating;
    if (key == 'u') isOrbiting = !isOrbiting;
    if (key == '1') {
        cameraMode = 1;
        glutSetCursor(GLUT_CURSOR_NONE);
    }
    if (key == '2') cameraMode = 2;
    if (key == '3') {
        cameraMode = 3;
        glutSetCursor(GLUT_CURSOR_NONE);
    }
    if (key == 't') {
        cameraMode = 5;
        glutSetCursor(GLUT_CURSOR_NONE);
        targetPlanetIndex = -1;
    }
}

void KeyboardUp(unsigned char key, int x, int y) {
    if (key >= 'A' && key <= 'Z') key = key + ('a' - 'A');
    keyState[key] = false;
}

void make_vertexShaders() {
    GLchar* vertexSource = filetobuf("vertex3.glsl");
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    free(vertexSource);
}

void make_fragmentShaders() {
    GLchar* fragmentSource = filetobuf("fragment3.glsl");
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    free(fragmentSource);
}

GLuint make_shaderProgram() {
    GLuint shaderID = glCreateProgram();
    glAttachShader(shaderID, vertexShader);
    glAttachShader(shaderID, fragmentShader);
    glLinkProgram(shaderID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glUseProgram(shaderID);
    return shaderID;
}
GLvoid Reshape(int w, int h)
{
    width = w;
    height = h;
    glViewport(0, 0, w, h);
}

void Timer(int value) {
    globalTime += 0.5f;
    if (missileFireCounter > 0) {
        missileFireCounter--;
    }
    if (isLeftButtonDown && missileFireCounter <= 0) {
        fireMissile();
        missileFireCounter = MISSILE_FIRE_DELAY;
    }

    if (isRotating) rotateTime += 0.5f;
    if (isOrbiting) orbitTime += 0.5f;

    // ★ 엔딩 시간 업데이트
    if (isEndingSequence) {
        endingTime += 0.02f;
    }

    updateMovement();
    updateGameLogic();
    updateParticles();
    glutPostRedisplay();
    glutTimerFunc(16, Timer, 0);
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
    int screenHeight = glutGet(GLUT_SCREEN_HEIGHT);
    width = screenWidth;
    height = screenHeight;
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(0, 0);
    glutInitWindowSize(width, height);
    glutCreateWindow("Solar System Adventure");
    glutFullScreen();
    glewExperimental = GL_TRUE;
    glewInit();
    glEnable(GL_DEPTH_TEST);

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();

    modelLoc = glGetUniformLocation(shaderProgramID, "model");
    viewLoc = glGetUniformLocation(shaderProgramID, "view");
    projLoc = glGetUniformLocation(shaderProgramID, "projection");
    colorLoc = glGetUniformLocation(shaderProgramID, "objectColor");
    lightPosLoc = glGetUniformLocation(shaderProgramID, "lightPos");
    viewPosLoc = glGetUniformLocation(shaderProgramID, "viewPos");
    isSunLoc = glGetUniformLocation(shaderProgramID, "isSun");

    initSphereData();
    initPlanetSettings();
    initStars();
    initAsteroids();
    initRing();
    whiteTex = createWhiteTexture();

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutKeyboardUpFunc(KeyboardUp);
    glutMouseFunc(Mouse);
    glutPassiveMotionFunc(Motion);
    glutMotionFunc(Motion);
    glutTimerFunc(16, Timer, 0);
    glutSetCursor(GLUT_CURSOR_NONE);

    glutMainLoop();

    return 0;
}