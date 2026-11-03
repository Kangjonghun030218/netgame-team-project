#define _CRT_SECURE_NO_WARNINGS
#include "connection_check.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // 프로젝트 폴더에 stb_image.h 파일이 있어야 합니다.

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

struct PlanetData {
    std::string name;      // [추가] 행성 이름 (예: "earth")
    float distance;
    float size;
    float orbitSpeed;
    float selfRotateSpeed; // 자전 속도
    float axisTilt;        // [추가] 자전축 기울기 (도 단위)
    GLuint textureID;

    // [추가] 행성 체력 (폭발 로직을 위해 추가)
    int health;
    float initialSize; // [추가] 크기 변화를 위해 초기 크기 저장
    bool active;

    std::string realDiameter;   // 실제 지름
    std::string realDay;        // 자전 주기 (1일 길이)
    std::string realYear;       // 공전 주기 (1년 길이)
    std::string planetType;     // 행성 타입 (암석/가스형)
};

bool operator==(const PlanetData& a, const PlanetData& b) {
    // 행성의 이름이 같으면 같은 행성으로 간주합니다.
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
    float yaw;   // [변경] 좌우 회전 각도 (기존 angle 대체)
    float pitch; // [추가] 상하 회전 각도
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
GLuint whiteTex; // [추가] 빈 텍스처 (하얀색)

// VAO/VBO
GLuint sphereVAO, sphereVBO, sphereEBO;
GLuint asteroidVAO, asteroidVBO;
GLuint starVAO, starVBO;
GLuint ringVAO, ringVBO; // 링 모델 데이터
GLuint saturnRingTex;    // 링 텍스처 ID

size_t sphereIndexCount = 0;

// 텍스처 ID들
GLuint sunTex, mercuryTex, venusTex, earthTex, moonTex, marsTex, jupiterTex, saturnTex, uranusTex, neptuneTex;
GLuint backgroundTex;

std::vector<PlanetData> planets;
std::vector<AsteroidData> asteroids;
std::vector<MissileData> missiles;
std::vector<Particle> particles;

const int asteroidCount = 1500;
const int starCount = 4000;
const float missileSpeed = 2.5f;
const float missileRange = 5000.0f;
const float PI = 3.1415926535f;

// 우주선(플레이어)
glm::vec3 shipPos(0.0f, 500.0f, 1500.0f);
float shipAngle = 180.0f;
float shipPitch = 0.0f;
bool keyState[256] = { false };

// 마우스 제어
float mouseSensitivity = 0.1f;
float lastMouseX = -1.0f, lastMouseY = -1.0f;
bool firstMouse = true;
bool isRightButtonDown = false;
int cameraMode = 1;
int targetPlanetIndex = -1; // 현재 추적 중인 행성의 인덱스
float mouse_x = 0.0f, mouse_y = 0.0f;


float globalTime = 0.0f;
// [추가] 자전/공전 제어 변수
bool isRotating = true;  // 자전 스위치 (기본 켜짐)
bool isOrbiting = true;  // 공전 스위치 (기본 켜짐)
float rotateTime = 0.0f; // 자전용 누적 시간
float orbitTime = 0.0f;  // 공전용 누적 시간


// [추가] 인게임 검색창 관련 변수
bool isTyping = false;       // 현재 검색창이 켜져있는가?
std::string inputString = ""; // 현재 입력된 검색어
bool isGameCleared = false;

const int MISSILE_FIRE_DELAY = 10;
int missileFireCounter = 0;


// --- 전역 변수 ---
// ... (기존 변수들 아래에 추가)

// [추가] 태양 폭발 연출용 변수
bool isSunExploding = false;    // 태양이 폭발 중인가?
bool isSunDestroyed = false;    // 태양이 완전히 사라졌는가?
float sunExplosionTimer = 0.0f; // 폭발 진행 시간

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

void spawnExplosion(glm::vec3 pos, glm::vec3 color, int count);
void spawnTrail(glm::vec3 pos, glm::vec3 color);

void updateParticles();
void updateGameLogic();
void updateMovement();
void fireMissile();

void drawScene();
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
// [추가] 하얀색 1x1 텍스처 생성 함수 (색상이 있는 물체용)
GLuint createWhiteTexture() {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // 하얀색 (R:255, G:255, B:255, A:255) 픽셀 데이터
    unsigned char whiteData[] = { 255, 255, 255, 255 };

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whiteData);

    // 텍스처 설정
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return textureID;
}
// [추가] 화면에 글자를 그리는 함수
void renderBitmapString(float x, float y, void* font, const char* string) {
    const char* c;
    glRasterPos2f(x, y);
    for (c = string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

// [추가] 검색창 UI를 그리는 함수
void drawInputBox() {
    if (!isTyping) return; // 입력 모드가 아니면 그리지 않음

    // 1. 3D 설정을 잠시 끄고 2D 모드로 전환
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height); // 화면 크기에 맞게 2D 좌표 설정

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST); // UI는 맨 앞에 보여야 하므로 깊이 검사 끔
    glDisable(GL_LIGHTING);   // UI는 조명 영향 안 받음
    glUseProgram(0);          // 쉐이더 끄기 (고정 파이프라인 사용)

    // 2. 반투명 검은색 박스 그리기 (화면 중앙)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.0f, 0.0f, 0.0f, 0.7f); // 검은색, 투명도 0.7
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float boxW = 300.0f; // 박스 너비 절반
    float boxH = 50.0f;  // 박스 높이 절반

    glBegin(GL_QUADS);
    glVertex2f(cx - boxW, cy - boxH);
    glVertex2f(cx + boxW, cy - boxH);
    glVertex2f(cx + boxW, cy + boxH);
    glVertex2f(cx - boxW, cy + boxH);
    glEnd();
    glDisable(GL_BLEND);

    // 3. 텍스트 그리기
    glColor3f(1.0f, 1.0f, 1.0f); // 흰색 글자

    // 안내 문구
    std::string title = "Enter Planet Name:";
    renderBitmapString(cx - 50, cy + 15, GLUT_BITMAP_HELVETICA_18, title.c_str());

    // 사용자가 입력 중인 글자 (커서 '_' 추가)
    std::string displayStr = inputString + "_";
    renderBitmapString(cx - 30, cy - 15, GLUT_BITMAP_HELVETICA_18, displayStr.c_str());

    // 4. 다시 3D 설정으로 복구
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glUseProgram(shaderProgramID); // 쉐이더 다시 켜기
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
        // 텍스처 로드 실패 시 콘솔에 메시지 출력
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

            data.push_back(x); data.push_back(y); data.push_back(z); // Pos
            data.push_back(x / radius); data.push_back(y / radius); data.push_back(z / radius); // Normal
            data.push_back((float)j / sectorCount); // U
            data.push_back((float)i / stackCount);  // V
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

    // Stride = 8 floats
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

            data.push_back(x); data.push_back(y); data.push_back(z); // Pos
            glm::vec3 n = glm::normalize(glm::vec3(x, y, z));
            data.push_back(n.x); data.push_back(n.y); data.push_back(n.z); // Normal
            data.push_back((float)j / sectorCount); // U
            data.push_back((float)i / stackCount);  // V
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
        // 1. 위치 (x, y, z) - 범위를 좀 넓게 퍼뜨림
        float x = (rand() % 5000 - 2000) / 5.0f;
        float y = (rand() % 5000 - 2000) / 5.0f;
        float z = (rand() % 5000 - 2000) / 5.0f;

        stars.push_back(x); stars.push_back(y); stars.push_back(z);

        // 2. 법선 (Normal) - 쉐이더 오류 방지용 가짜 데이터 (0, 1, 0)
        stars.push_back(0.0f); stars.push_back(1.0f); stars.push_back(0.0f);

        // 3. 텍스처 좌표 (UV) - 쉐이더 오류 방지용 가짜 데이터 (0, 0)
        stars.push_back(0.0f); stars.push_back(0.0f);
    }

    // VAO, VBO 생성
    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);

    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(float), &stars[0], GL_STATIC_DRAW);

    // ★ 쉐이더 포맷 맞추기 (8개의 float가 한 세트) ★

    // 0번: 위치 (3개)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 1번: 법선 (3개) - 이게 없으면 꺼짐
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 2번: 텍스처 (2개) - 이게 없으면 꺼짐
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // 안전하게 VAO 바인딩 해제
    glBindVertexArray(0);
}
void initPlanetSettings() {
    // 텍스처 로딩 (기존과 동일)
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

    // 행성 크기: 잘 보이게 1.2배 정도 (태양 대비 너무 작아 보이지 않게)
    float baseScale = 1.2f;

    float s_mer = 0.38f * baseScale;
    float s_ven = 0.95f * baseScale;
    float s_ear = 1.0f * baseScale;
    float s_mar = 0.53f * baseScale;
    float s_jup = 11.21f * baseScale;
    float s_sat = 9.45f * baseScale;
    float s_ura = 4.01f * baseScale;
    float s_nep = 3.88f * baseScale;

    // ★★★ [거리 대폭 수정] 태양 크기(30)를 고려하여 멀리 배치 ★★★
    // 거리: 60, 100, 140... 씩 띄움

    // 1. 수성 (거리 60)
    planets.push_back({ "mercury", 60.0f, s_mer, 365.0f / 88.0f, 1.0f / 58.65f, 0.03f, mercuryTex, (int)(s_mer * 10.0f), s_mer, true,
        "4,879 km", "58.6 Days", "88 Days", "Terrestrial Planet" });

    // 2. 금성 (거리 100)
    planets.push_back({ "venus", 100.0f, s_ven, 365.0f / 226.0f, 1.0f / -243.01f, 177.3f, venusTex, (int)(s_ven * 10.0f), s_ven, true,
        "12,104 km", "243 Days (Retrograde)", "225 Days", "Terrestrial Planet" });

    // 3. 지구 (거리 140)
    planets.push_back({ "earth", 140.0f, s_ear, 1.0f, 1.0f, 23.5f, earthTex, (int)(s_ear * 10.0f), s_ear, true,
        "12,742 km", "23.9 Hours", "365.25 Days", "Terrestrial Planet" });

    // 4. 화성 (거리 180)
    planets.push_back({ "mars", 180.0f, s_mar, 365.0f / 686.0f, 1.0f / 1.026f, 25.2f, marsTex, (int)(s_mar * 10.0f), s_mar, true,
        "6,779 km", "24.6 Hours", "687 Days", "Terrestrial Planet" });

    // 5. 목성 (거리 300 - 여기서부터 확 멀어짐)
    planets.push_back({ "jupiter", 300.0f, s_jup, 365.0f / 4343.5f * 5.0f, 1.0f / 0.41f, 3.1f, jupiterTex, (int)(s_jup * 10.0f), s_jup, true,
        "139,820 km", "9.9 Hours", "11.86 Years", "Gas Giant" });

    // 6. 토성 (거리 450)
    planets.push_back({ "saturn", 450.0f, s_sat, 365.0f / 10731.0f * 5.0f, 1.0f / 0.426f, 26.7f, saturnTex, (int)(s_sat * 10.0f), s_sat, true,
        "116,460 km", "10.7 Hours", "29.45 Years", "Gas Giant" });

    // 7. 천왕성 (거리 600)
    planets.push_back({ "uranus", 600.0f, s_ura, 365.0f / 30587.0f * 10.0f, 1.0f / -0.718f, 97.8f, uranusTex, (int)(s_ura * 10.0f), s_ura, true,
        "50,724 km", "17.2 Hours (Retrograde)", "84 Years", "Ice Giant" });

    // 8. 해왕성 (거리 750)
    planets.push_back({ "neptune", 750.0f, s_nep, 365.0f / 59787.0f * 10.0f, 1.0f / 0.67f, 28.3f, neptuneTex, (int)(s_nep * 10.0f), s_nep, true,
        "49,244 km", "16.1 Hours", "164.8 Years", "Ice Giant" });
}
// [추가] 특정 인덱스 행성의 현재 (x, y, z) 위치를 계산하여 반환하는 함수
glm::vec3 getPlanetPosition(int index) {
    if (index < 0 || index >= planets.size()) return glm::vec3(0, 0, 0);

    float angle = orbitTime * planets[index].orbitSpeed; // 현재 공전 각도
    // 행성은 (distance, 0, 0)에서 Y축 기준 회전함
    // 회전 행렬을 직접 계산하거나 삼각함수 사용
    // glm::rotate는 라디안 단위를 씁니다.

    // 단순화된 계산 (y축 회전)
    float rad = glm::radians(angle);
    float x = cos(rad) * planets[index].distance;
    float z = -sin(rad) * planets[index].distance;

    return glm::vec3(x, 0.0f, z);
}

void initAsteroids() {
    asteroids.clear();
    for (int i = 0; i < asteroidCount; ++i) {
        AsteroidData a;
        float range = 450.0f;
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
    glm::vec3 moveDir(0.0f);
    glm::vec3 front;
    front.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    front.y = sin(glm::radians(shipPitch));
    front.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    front = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::normalize(glm::cross(right, front));

    if (keyState['w']) moveDir += front;
    if (keyState['s']) moveDir -= front;
    if (keyState['a']) moveDir -= right;
    if (keyState['d']) moveDir += right;
    /*if (keyState['q']) moveDir += up;
    if (keyState['e']) moveDir -= up;*/

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

// [추가] 조준선(Crosshair) 그리기 함수
void drawCrosshair() {
    // 1인칭(1) 또는 3인칭(3)이 아닐 때는 그리지 않음
    if (cameraMode != 1 && cameraMode != 3) return;

    // 1. 2D 모드로 전환 (UI 그리기 위함)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 2. 쉐이더 및 3D 설정 끄기
    glUseProgram(0);          // 쉐이더 해제
    glDisable(GL_DEPTH_TEST); // 깊이 테스트 끄기 (맨 앞에 보임)
    glDisable(GL_LIGHTING);   // 조명 끄기

    // 3. 조준선 그리기 (녹색 십자가)
    float cx = width / 2.0f;  // 화면 중심 X
    float cy = height / 2.0f; // 화면 중심 Y
    float size = 15.0f;       // 십자가 길이
    float gap = 5.0f;         // 중앙 빈 공간 (시야 확보용)

    glLineWidth(2.0f);        // 선 두께 설정
    glColor3f(0.0f, 1.0f, 0.0f); // 녹색 (Green)

    glBegin(GL_LINES);
    // 가로 선 (좌우)
    glVertex2f(cx - size, cy);
    glVertex2f(cx - gap, cy);
    glVertex2f(cx + gap, cy);
    glVertex2f(cx + size, cy);

    // 세로 선 (상하)
    glVertex2f(cx, cy - size);
    glVertex2f(cx, cy - gap);
    glVertex2f(cx, cy + gap);
    glVertex2f(cx, cy + size);
    glEnd();

    glLineWidth(1.0f); // 선 두께 원상복구

    // 4. 3D 설정 복구
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(shaderProgramID); // 쉐이더 다시 켜기

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void fireMissile() {
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

    // ★★★ [수정 1] 조준점 좌표 계산 (1인칭은 무조건 화면 중앙) ★★★
    float winX, winY;
    if (cameraMode == 1) {
        winX = (float)viewport[2] / 2.0f; // 화면 너비 절반
        winY = (float)viewport[3] / 2.0f; // 화면 높이 절반
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

    // (충돌 검사 로직은 기존 유지)
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

    // ★★★ [수정 2] 3D 회전 각도(Yaw, Pitch) 계산 ★★★
    MissileData m;
    glm::vec3 fireDir = glm::normalize(targetPos - shipPos);
    m.position = shipPos + fireDir * 2.0f;
    m.velocity = fireDir * missileSpeed;

    // 좌우 회전 (Y축 기준)
    m.yaw = glm::degrees(atan2(fireDir.x, fireDir.z));
    // 상하 회전 (X축 기준)
    m.pitch = glm::degrees(asin(fireDir.y));

    m.lifeTime = 0.0f;
    m.active = true;
    missiles.push_back(m);
}


void initRing() {
    std::vector<float> data;
    const int segments = 72; // 원을 몇 조각으로 나눌지 (많을수록 부드러움)
    float innerRadius = 0.6f; // 안쪽 반지름 (행성보다 약간 커야 함)
    float outerRadius = 1.2f; // 바깥쪽 반지름

    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * 2.0f * PI;
        float c = cosf(angle);
        float s = sinf(angle);

        // 안쪽 점 (Inner) -> 텍스처의 왼쪽(0.0)
        data.push_back(c * innerRadius); data.push_back(0.0f); data.push_back(s * innerRadius); // Pos
        data.push_back(0.0f); data.push_back(1.0f); data.push_back(0.0f);                       // Normal (위쪽)
        data.push_back(0.0f); data.push_back(0.0f);                                             // TexCoord (시작)

        // 바깥쪽 점 (Outer) -> 텍스처의 오른쪽(1.0)
        data.push_back(c * outerRadius); data.push_back(0.0f); data.push_back(s * outerRadius); // Pos
        data.push_back(0.0f); data.push_back(1.0f); data.push_back(0.0f);                       // Normal (위쪽)
        data.push_back(1.0f); data.push_back(0.0f);                                             // TexCoord (끝)
    }

    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);

    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);

    // 쉐이더 속성 연결 (Pos, Normal, TexCoord)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

void updateGameLogic() {
    for (int i = 0; i < missiles.size(); ) {
        if (!missiles[i].active) {
            missiles.erase(missiles.begin() + i);
            continue;
        }
        missiles[i].position += missiles[i].velocity;
        if (glm::length(missiles[i].position) > missileRange) missiles.erase(missiles.begin() + i);
        else i++;
    }

    for (auto& m : missiles) {
        if (!m.active) continue;
        // ----------------------------------------------------
        // [추가] 행성과의 충돌 처리
        // ----------------------------------------------------
        for (auto& p : planets) {
            // 태양은 제외 (거리 0.0f인 경우)
            if (p.distance == 0.0f) continue;

            // 행성의 현재 위치 계산 (공전 위치)
            glm::vec3 currentPlanetPos = getPlanetPosition(std::distance(planets.begin(), std::find(planets.begin(), planets.end(), p)));

            // 미사일과 행성 중심 간의 거리
            float dist = glm::length(m.position - currentPlanetPos);

            // 충돌 반경 설정 (행성 크기 + 미사일 여유분 0.5f)
            if (dist < p.size + 0.5f) {
                m.active = false; // 미사일 비활성화
                p.health -= 10;       // 행성 체력 감소
                spawnExplosion(m.position, glm::vec3(0.1f, 0.5f, 0.2f), 5); // 충돌 폭발 효과 (색상 변경)

                // 행성 크기 감소 및 완전 파괴
                float initialHealth = p.initialSize * 10.0f;
                float hpRatio = (float)p.health / initialHealth;
                if (p.health > 0) {
                    // 체력에 비례하여 크기 줄이기 (최소 50%까지)
                    p.size = p.initialSize * (0.5f + 0.5f * hpRatio);
                }
                else {
                    p.size = 0.0f; // 완전 파괴
                    p.health = 0;
                    spawnExplosion(currentPlanetPos, glm::vec3(1.0f, 0.1f, 0.0f), 100); // 대규모 폭발
                    printf("Planet %s was destroyed!\n", p.name.c_str());
                }
                break;
            }
        }
        for (auto& a : asteroids) {
            if (a.health <= 0) continue;
            glm::vec3 currentAsteroidPos = a.position + (a.velocity * globalTime);
            float dist = glm::length(m.position - currentAsteroidPos);
            if (dist < a.size + 0.3f) {
                m.active = false;
                a.health--;
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

    if (!isGameCleared && !isSunDestroyed) { // 아직 클리어 안 했고 태양도 살아있다면
        int activePlanetCount = 0;
        for (const auto& p : planets) {
            if (p.distance != 0.0f && p.size > 0.001f) {
                activePlanetCount++;
            }
        }

        // 모든 행성이 파괴되었고, 아직 폭발이 시작되지 않았다면 -> 폭발 시작!
        if (activePlanetCount == 0 && !isSunExploding) {
            isSunExploding = true;
            printf("WARNING: SUN IS UNSTABLE!\n");
        }
    }

    // [추가] 태양 폭발 애니메이션 로직
    if (isSunExploding) {
        sunExplosionTimer += 0.1f; // 폭발 진행

        // 1. 지속적으로 태양 위치에서 파티클 생성 (점점 격렬하게)
        int particleCount = 5 + (int)(sunExplosionTimer * 2.0f);
        glm::vec3 sunColor = glm::mix(glm::vec3(1.0f, 0.5f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), sunExplosionTimer / 10.0f); // 주황 -> 흰색
        spawnExplosion(glm::vec3(0, 0, 0), sunColor, particleCount);

        // 2. 일정 시간이 지나면 태양 소멸 및 게임 클리어 처리
        if (sunExplosionTimer > 15.0f) { // 약 3초 정도 후 (타이머 속도에 따라 조절)
            isSunExploding = false;
            isSunDestroyed = true;
            isGameCleared = true; // 이때 텍스트가 뜨게 됨

            // 마지막 대폭발 이펙트
            spawnExplosion(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), 500);
            printf("\n\n*** SUN DESTROYED! UNIVERSE IS DOOMED! ***\n\n");

            isRotating = false;
            isOrbiting = false;
        }
    }
}
bool isLeftButtonDown = false;
void Mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            isLeftButtonDown = true;
            mouse_x = x;
            mouse_y = y;
        }
        else if (state == GLUT_UP) {
            isLeftButtonDown = false;
        }
    }


}

void drawPlanetInfo() {
    if (cameraMode != 4 || targetPlanetIndex == -1) return;
    if (targetPlanetIndex >= planets.size()) return;

    PlanetData& p = planets[targetPlanetIndex];

    // 1. 2D 설정
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

    // 2. 배경 박스 (좀 더 크게 조정)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.7f); // 좀 더 진하게

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

    // 3. 실제 데이터 출력
    glColor3f(1.0f, 1.0f, 1.0f); // 흰색

    char buffer[256];
    float textX = startX + 15.0f;
    float textY = startY - 30.0f;
    float lineGap = 25.0f;

    // (1) 이름 (대문자 강조)
    glColor3f(0.3f, 0.8f, 1.0f); // 제목은 밝은 하늘색
    std::string nameUpper = p.name;
    std::transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::toupper);
    sprintf(buffer, "=== %s ===", nameUpper.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= (lineGap + 5);

    glColor3f(0.9f, 0.9f, 0.9f); // 내용은 밝은 회색

    // (2) 행성 분류
    sprintf(buffer, "Type: %s", p.planetType.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_12, buffer);
    textY -= lineGap;

    // (3) 실제 지름
    sprintf(buffer, "Real Diameter: %s", p.realDiameter.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= lineGap;

    // (4) 자전 주기 (하루 길이)
    sprintf(buffer, "Day Length: %s", p.realDay.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= lineGap;

    // (5) 공전 주기 (1년 길이)
    sprintf(buffer, "Year Length: %s", p.realYear.c_str());
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= lineGap;

    // (6) 자전축 기울기
    sprintf(buffer, "Axial Tilt: %.1f deg", p.axisTilt);
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);
    textY -= (lineGap + 5);

    // (7) 현재 상태 (HP) - 게임 정보도 살짝 표시
    if (p.health > 50) glColor3f(0.2f, 1.0f, 0.2f);
    else if (p.health > 20) glColor3f(1.0f, 0.8f, 0.2f);
    else glColor3f(1.0f, 0.2f, 0.2f);

    sprintf(buffer, "[ Status: HP %d%% ]", (int)((float)p.health / (p.initialSize * 10.0f) * 100.0f));
    renderBitmapString(textX, textY, GLUT_BITMAP_HELVETICA_18, buffer);


    // 4. 복구
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(shaderProgramID);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Motion(int x, int y) {
    // [수정] 1인칭 모드(cameraMode == 1)일 때만 shouldRotate가 true가 되며 커서 고정/숨김 로직이 실행됩니다.
    // 참고: isTyping 중에는 Motion이 호출되지 않도록 Keyboard 함수에서 막고 있습니다.

    bool shouldRotate = (cameraMode == 1 || cameraMode == 3 || isLeftButtonDown);

    if (firstMouse) { lastMouseX = x; lastMouseY = y; firstMouse = false; return; }

    if (shouldRotate) {
        mouse_x = x;
        mouse_y = y;
        float xoffset, yoffset;

        // 1인칭 모드일 때 마우스 커서를 중앙으로 워프시켜 무한 회전을 가능하게 합니다.
        if (cameraMode == 3 || cameraMode == 1) {
            int centerX = width / 2;
            int centerY = height / 2;

            xoffset = x - centerX;
            yoffset = y - centerY;

            // 중앙에 고정
            if (x != centerX || y != centerY) {
                glutWarpPointer(centerX, centerY);
            }
            // 1인칭 모드에서는 lastMouseX, Y는 더 이상 사용되지 않고 centerX, centerY가 기준이 됩니다.
        }
        else {
            // 그 외 모드에서는 기존 로직 (드래그)
            xoffset = x - lastMouseX;
            yoffset = y - lastMouseY;
            lastMouseX = x;
            lastMouseY = y;
        }

        // 오프셋이 0이 아니거나, 1인칭이 아닌 모드에서 회전 중일 때만 업데이트
        if (xoffset != 0.0f || yoffset != 0.0f) {
            shipAngle -= xoffset * mouseSensitivity;
            shipPitch -= yoffset * mouseSensitivity;
            if (shipPitch > 89.0f) shipPitch = 89.0f;
            if (shipPitch < -89.0f) shipPitch = -89.0f;
            glutPostRedisplay();
        }
    }
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

    if (cameraMode == 1) { cameraPos = shipPos + glm::vec3(0, 0.5, 0) + shipDir * 2.0f; cameraTarget = cameraPos + shipDir; }
    else if (cameraMode == 2) { cameraPos = glm::vec3(0, 800, 10); cameraTarget = glm::vec3(0, 0, 0); cameraUp = glm::vec3(0, 0, -1); }
    else if (cameraMode == 4 && targetPlanetIndex != -1) {
        // 타겟 행성의 현재 위치 계산
        glm::vec3 planetPos = getPlanetPosition(targetPlanetIndex);

        // 카메라는 행성 위치에서 (5, 3, 5) 정도 떨어진 곳에 배치
        // 행성 크기에 비례해서 멀리 떨어지게 하면 더 좋습니다.
        float distOffset = planets[targetPlanetIndex].size * 4.0f + 5.0f;

        cameraPos = planetPos + glm::vec3(distOffset, distOffset * 0.5f, distOffset);
        cameraTarget = planetPos; // 카메라는 행성을 바라봄
    }
    else if (cameraMode == 5) {
        // 1. 카메라 위치: 태양(0,0,0)의 바로 위
        // 태양 반지름이 30이니까, Y=150 정도면 적당히 높습니다.
        cameraPos = glm::vec3(0.0f, 150.0f, 0.0f);

        // 2. 바라보는 곳: 시간(globalTime)에 따라 회전
        float rotateSpeed = globalTime * 0.2f; // 회전 속도 (숫자가 작을수록 느림)
        float viewRadius = 1000.0f; // 멀리 바라봄

        // 원을 그리며 회전하는 타겟 좌표 계산
        cameraTarget.x = cos(glm::radians(rotateSpeed)) * viewRadius;
        cameraTarget.z = sin(glm::radians(rotateSpeed)) * viewRadius;

        // Y값을 0보다 낮게(-200) 주면 살짝 아래를 내려다보게 되어 궤도가 더 잘 보입니다.
        cameraTarget.y = -200.0f;

        cameraUp = glm::vec3(0, 1, 0);
    }
    else { cameraPos = shipPos - (shipDir * 8.0f) + glm::vec3(0, 4, 0); cameraTarget = shipPos + shipDir * 5.0f; }

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

    // [1] 별 그리기
    // ---------------------------------------------------------
    glUniform1i(isSunLoc, 1);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, whiteTex);

    glPointSize(2.0f);
    glBindVertexArray(starVAO); // ★ 여기서 '별' 모델을 선택함
    glDrawArrays(GL_POINTS, 0, starCount); // 별 그리기

    // ---------------------------------------------------------
    // [2] ★★★ 핵심 수정 사항 ★★★
    // 별 그리기가 끝났으니, 다시 '구(행성)' 모델로 돌아와야 합니다!
    // 이 줄이 없으면 태양을 그릴 때 꺼집니다.
    // ---------------------------------------------------------
    glBindVertexArray(sphereVAO);

    // ★★★ 여기가 제일 중요합니다! ★★★
    // glDrawElements가 아니라 "glDrawArrays"여야 합니다.
    // 또한 sphereIndexCount가 아니라 "starCount"여야 합니다.
    glDrawArrays(GL_POINTS, 0, starCount);


    if (!isSunDestroyed) { // [수정] 태양이 파괴되지 않았을 때만 그림

        float sunScale = 30.0f; // 기본 크기

        if (isSunExploding) {
            // [추가] 폭발 중이면 크기가 기하급수적으로 커짐
            // 진동 효과 (Shake) 추가
            float shakeX = (rand() % 100 - 50) / 50.0f * (sunExplosionTimer * 0.5f);
            float shakeY = (rand() % 100 - 50) / 50.0f * (sunExplosionTimer * 0.5f);
            float shakeZ = (rand() % 100 - 50) / 50.0f * (sunExplosionTimer * 0.5f);

            // 시간이 지날수록 크기 증가 (초반엔 천천히, 나중엔 빠르게)
            sunScale = 30.0f + (sunExplosionTimer * sunExplosionTimer * 2.0f);

            glm::mat4 sunModel = glm::translate(glm::mat4(1.0f), glm::vec3(shakeX, shakeY, shakeZ));
            sunModel = glm::rotate(sunModel, glm::radians(rotateTime * 0.2f), glm::vec3(0, 1, 0));
            sunModel = glm::scale(sunModel, glm::vec3(sunScale));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sunModel));

            // 폭발 중에는 색상이 점점 하얗게 변함
            float whiteness = std::min(sunExplosionTimer / 10.0f, 1.0f);
            glUniform4f(colorLoc, 1.0f, 1.0f - whiteness * 0.5f, 1.0f - whiteness * 0.5f, 1.0f);
        }
        else {
            // 평상시 태양
            glm::mat4 sunModel = glm::rotate(glm::mat4(1.0f), glm::radians(rotateTime * 0.2f), glm::vec3(0, 1, 0));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::scale(sunModel, glm::vec3(30.0f))));
            glUniform4f(colorLoc, 1, 1, 1, 1);
        }

        glBindTexture(GL_TEXTURE_2D, sunTex);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }

    // 행성
    for (auto& p : planets) {
        glUniform1i(isSunLoc, 1); // 궤도선
        glBindTexture(GL_TEXTURE_2D, whiteTex); // [수정] 흰색 텍스처 사용
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniform4f(colorLoc, 0.2f, 0.2f, 0.4f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for (int a = 0; a < 360; a++) glVertex3f(cos(glm::radians((float)a)) * p.distance, 0, sin(glm::radians((float)a)) * p.distance);
        glEnd();
        if (p.size < 0.001f) continue;
        glUniform1i(isSunLoc, 0); // 행성 본체
        // [수정] 공전 계산: globalTime -> orbitTime 사용
        glm::mat4 pModel = glm::rotate(glm::mat4(1.0f), glm::radians(orbitTime * p.orbitSpeed), glm::vec3(0, 1, 0));
        pModel = glm::translate(pModel, glm::vec3(p.distance, 0, 0));
        glm::mat4 earthPos = pModel; // 달 위치 저장용

        // ★★★ [수정 파트 시작] 자전축 기울기 및 자전 적용 ★★★

      // [중요] 기울기를 먼저 적용해야 올바르게 돕니다.
        // 1. 자전축 기울기 (Z축 회전)
        pModel = glm::rotate(pModel, glm::radians(p.axisTilt), glm::vec3(0, 0, 1));

        // 2. 자전 (Y축 회전)
        // 표의 데이터가 반영된 자전 속도 적용
        pModel = glm::rotate(pModel, glm::radians(rotateTime * p.selfRotateSpeed * 5.0f), glm::vec3(0, 1, 0));
        // *5.0f는 전체적인 회전 속도를 조절하는 계수입니다. (너무 느리면 숫자를 키우세요)

        // 3. 크기 적용
        pModel = glm::scale(pModel, glm::vec3(p.size));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
        glUniform4f(colorLoc, 1, 1, 1, 1);
        glBindTexture(GL_TEXTURE_2D, p.textureID);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        if (p.name == "earth" && p.size > 0.0f) {
            glm::mat4 mModel = glm::rotate(earthPos, glm::radians(orbitTime * 3.0f), glm::vec3(0, 1, 0));
            mModel = glm::translate(mModel, glm::vec3(3.0f, 0, 0)); // 달과 지구 사이 거리도 살짝 벌림 (1.0 -> 3.0)
            mModel = glm::scale(mModel, glm::vec3(0.25f)); // 달 크기 조절

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mModel));
            glBindTexture(GL_TEXTURE_2D, moonTex);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
        if (p.name == "saturn" && p.size > 0.0f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            glUniform1i(isSunLoc, 1); // 조명 무시 (밝게)

            // 고리 크기 설정
            glm::mat4 rModel = glm::scale(pModel, glm::vec3(2.0f, 2.0f, 2.0f)); // 고리 크기 배율
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(rModel));

            glUniform4f(colorLoc, 1.2f, 1.2f, 1.2f, 0.8f); // 약간 투명하게

            glBindTexture(GL_TEXTURE_2D, saturnRingTex);
            glBindVertexArray(ringVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, (72 + 1) * 2);

            glUniform1i(isSunLoc, 0); // 조명 복구

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glBindVertexArray(sphereVAO); // 다시 구 모델로 복구
        }
    }

    // 운석
    glUniform1i(isSunLoc, 0);
    glBindVertexArray(asteroidVAO);
    glBindTexture(GL_TEXTURE_2D, moonTex); // 운석 텍스처
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

    // ================== [수정됨] 미사일 그리기 (그럴듯한 모양) ==================
    glUniform1i(isSunLoc, 1);
    glBindVertexArray(sphereVAO);
    glBindTexture(GL_TEXTURE_2D, whiteTex); // [수정] 흰색 텍스처 사용

    for (const auto& m : missiles) {
        // 1. 미사일의 진행 방향(Direction) 계산
        glm::vec3 dir = glm::normalize(m.velocity);

        // 2. 진행 방향을 바라보는 회전 행렬(Model Matrix) 생성 (★핵심★)
        // glm::lookAt은 '카메라'가 보는 행렬이므로, 이를 '뒤집으면(inverse)' 물체가 그 방향을 보는 행렬이 됩니다.
        // 위치(m.position)에서 -> 목표지점(m.position + dir)을 바라보게 만듦
        // 등(Up)은 항상 (0,1,0) 하늘을 향하게 해서 기우뚱하지 않게 함
        glm::mat4 missileModel = glm::inverse(glm::lookAt(m.position, m.position + dir, glm::vec3(0.0f, 1.0f, 0.0f)));

        // 3. 모델 방향 보정 (필요한 경우)
        // 미사일 모델이 원래 -Z(앞)가 아니라 +Z(뒤)를 보고 만들어졌다면 거꾸로 날아갑니다.
        // 만약 미사일이 꼬리부터 날아가면 아래 주석을 풀어서 180도 돌려주세요.
        missileModel = glm::rotate(missileModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // --- 미사일 몸체 ---
        glm::mat4 bodyModel = glm::scale(missileModel, glm::vec3(0.1f, 0.1f, 0.8f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bodyModel));
        glUniform4f(colorLoc, 0.2f, 0.8f, 1.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // --- 미사일 탄두 ---
        glm::mat4 headModel = glm::translate(missileModel, glm::vec3(0.0f, 0.0f, 0.8f)); // 몸체 앞쪽으로(Z축)
        // *주의: 여기서 translate는 이미 회전된 축 기준(Local)으로 이동하므로 Z축으로 이동하면 됨
        headModel = glm::scale(headModel, glm::vec3(0.1f, 0.1f, 0.3f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(headModel));
        glUniform4f(colorLoc, 1.0f, 0.2f, 0.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // --- 미사일 날개 ---
        for (int i = 0; i < 4; ++i) {
            glm::mat4 finModel = glm::rotate(missileModel, glm::radians(i * 90.0f), glm::vec3(0, 0, 1));
            finModel = glm::translate(finModel, glm::vec3(0.15f, 0.0f, -0.6f));
            finModel = glm::scale(finModel, glm::vec3(0.15f, 0.02f, 0.3f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(finModel));
            glUniform4f(colorLoc, 0.6f, 0.6f, 0.6f, 1.0f);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
    }

    // ================== [수정됨] 우주선 그리기 (그럴듯한 모양) ==================
    if (cameraMode != 1) {
        glBindTexture(GL_TEXTURE_2D, whiteTex); // [수정] 흰색 텍스처 사용

        // 1. 우주선 기본 변환 행렬 (위치, 회전 적용)
        glm::mat4 shipBaseModel = glm::mat4(1.0f);
        shipBaseModel = glm::translate(shipBaseModel, shipPos);
        shipBaseModel = glm::rotate(shipBaseModel, glm::radians(shipAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        // 위아래 회전 (Pitch) 적용
        shipBaseModel = glm::rotate(shipBaseModel, glm::radians(-shipPitch), glm::vec3(1.0f, 0.0f, 0.0f));
        shipBaseModel = glm::scale(shipBaseModel, glm::vec3(0.5f)); // 전체 크기 조절

        // --- 본체 (길쭉한 몸통) ---
        glm::mat4 bodyModel = glm::scale(shipBaseModel, glm::vec3(0.7f, 0.5f, 2.5f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bodyModel));
        glUniform4f(colorLoc, 0.6f, 0.6f, 0.7f, 1.0f); // 회색 본체
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // --- 조종석 (파란색 유리창 느낌) ---
        glm::mat4 cockpitModel = glm::translate(shipBaseModel, glm::vec3(0.0f, 0.3f, 0.5f)); // 약간 위쪽, 앞쪽으로
        cockpitModel = glm::scale(cockpitModel, glm::vec3(0.4f, 0.3f, 0.8f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(cockpitModel));
        glUniform4f(colorLoc, 0.2f, 0.6f, 1.0f, 1.0f); // 파란색 조종석
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // --- 날개 (양옆으로 넓게) ---
        // 왼쪽 날개
        glm::mat4 leftWingModel = glm::translate(shipBaseModel, glm::vec3(-1.2f, 0.0f, -0.5f));
        leftWingModel = glm::scale(leftWingModel, glm::vec3(1.5f, 0.1f, 1.2f));
        leftWingModel = glm::rotate(leftWingModel, glm::radians(20.0f), glm::vec3(0, 0, 1)); // 약간 기울임
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(leftWingModel));
        glUniform4f(colorLoc, 0.7f, 0.2f, 0.2f, 1.0f); // 빨간색 날개
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // 오른쪽 날개 (대칭)
        glm::mat4 rightWingModel = glm::translate(shipBaseModel, glm::vec3(1.2f, 0.0f, -0.5f));
        rightWingModel = glm::scale(rightWingModel, glm::vec3(1.5f, 0.1f, 1.2f));
        rightWingModel = glm::rotate(rightWingModel, glm::radians(-20.0f), glm::vec3(0, 0, 1)); // 반대로 기울임
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(rightWingModel));
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0); // 색상은 위에서 설정한 빨간색 그대로

        // --- 엔진 (뒤쪽에 두 개) ---
        for (int i = -1; i <= 1; i += 2) {
            glm::mat4 engineModel = glm::translate(shipBaseModel, glm::vec3(0.6f * i, 0.0f, -1.5f)); // 양옆, 뒤쪽
            engineModel = glm::scale(engineModel, glm::vec3(0.3f, 0.3f, 0.8f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(engineModel));
            glUniform4f(colorLoc, 0.3f, 0.3f, 0.3f, 1.0f); // 진한 회색 엔진
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

            // 엔진 노즐 (주황색 불꽃 나오는 부분)
            glm::mat4 nozzleModel = glm::translate(engineModel, glm::vec3(0.0f, 0.0f, -1.0f)); // 엔진 뒤쪽
            nozzleModel = glm::scale(nozzleModel, glm::vec3(0.8f, 0.8f, 0.2f)); // 약간 작고 납작하게
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(nozzleModel));
            glUniform4f(colorLoc, 1.0f, 0.5f, 0.0f, 1.0f); // 주황색 노즐
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
    }

    // 파티클
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glUniform1i(isSunLoc, 1);
    glBindVertexArray(sphereVAO);
    glBindTexture(GL_TEXTURE_2D, whiteTex); // [수정] 흰색 텍스처 사용
    for (const auto& p : particles) {
        glm::mat4 pModel = glm::scale(glm::translate(glm::mat4(1.0f), p.position), glm::vec3(p.size));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
        glUniform4f(colorLoc, p.color.r, p.color.g, p.color.b, p.life);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    if (isGameCleared) {
        // 1. 2D 모드로 전환
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, width, 0, height);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glDisable(GL_DEPTH_TEST);
        glUseProgram(0); // 쉐이더 끄기

        // ★★★ [해결책] 조명과 텍스처를 꺼야 색상이 제대로 나옵니다 ★★★
        glDisable(GL_LIGHTING);   // 조명 끄기 (이게 켜져 있으면 검게 나옴)
        glDisable(GL_TEXTURE_2D); // 텍스처 끄기 (혹시 텍스처가 섞이는 것 방지)

        // 2. 텍스트 설정 및 출력
        glColor3f(1.0f, 0.0f, 0.0f); // 빨간색 (이제 잘 보일 겁니다)

        float cx = width / 2.0f;
        float cy = height / 2.0f;

        std::string winText = "G A M E   C L E A R E D !";
        glRasterPos2f(cx - 140, cy);
        renderBitmapString(cx - 140, cy, GLUT_BITMAP_TIMES_ROMAN_24, winText.c_str());

        // 3. 3D 설정으로 복구 (다시 켜주는 것을 잊지 마세요)
        glEnable(GL_TEXTURE_2D); // 텍스처 다시 켜기 (필요하다면)
        glEnable(GL_LIGHTING);   // 조명 다시 켜기
        glEnable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glUseProgram(shaderProgramID);
    }
    drawCrosshair();
    drawPlanetInfo();
    drawInputBox();
    glutSwapBuffers();
}

void Keyboard(unsigned char key, int x, int y) {
    // 1. 입력 모드일 때 (글자 타이핑 중)
    if (isTyping) {
        if (key == 13) { // [Enter] 키: 검색 실행
            bool found = false;
            for (int i = 0; i < planets.size(); ++i) {
                if (planets[i].name == inputString) {
                    targetPlanetIndex = i;
                    cameraMode = 4; // 행성 추적 모드
                    found = true;
                    break;
                }
            }
            if (!found) {
                printf("Planet '%s' not found!\n", inputString.c_str());
                // 못 찾았어도 일단 창은 닫거나, 혹은 유지하거나 선택 가능
                // 여기선 그냥 닫겠습니다.
            }
            isTyping = false; // 입력창 닫기
            inputString = ""; // 내용 초기화
        }
        else if (key == 8) { // [Backspace] 키: 글자 지우기
            if (inputString.length() > 0) {
                inputString.pop_back();
            }
        }
        else if (key == 27) { // [ESC] 키: 취소
            isTyping = false;
            inputString = "";
        }
        else if (key >= 32 && key <= 126) { // 일반 문자 입력
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
    if (key == '1')
    {
        cameraMode = 1;
        glutSetCursor(GLUT_CURSOR_NONE);
    }
    if (key == '2') cameraMode = 2;
    if (key == '3')
    {
        cameraMode = 3;
        glutSetCursor(GLUT_CURSOR_NONE);
    }
    if (key == 't' || key == 'T') {
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

    updateMovement();
    updateGameLogic();
    updateParticles();
    glutPostRedisplay();
    glutTimerFunc(16, Timer, 0);
}

int main(int argc, char** argv)
{
    static ConnectionCheck connection;
    connection.connectTo("127.0.0.1", 9000);

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