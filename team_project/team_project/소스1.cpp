#define _CRT_SECURE_NO_WARNINGS
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

// --- 구조체 정의 ---
struct PlanetData {
    float distance;
    float size;
    float orbitSpeed;
    float selfRotateSpeed;
    GLuint textureID;
};

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
    float angle;
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
GLuint rimColorLoc; // [추가] 림 라이팅 색상 위치 변수

int sphereIndexCount = 0;

// 텍스처 ID들
GLuint sunTex, mercuryTex, venusTex, earthTex, moonTex, marsTex, jupiterTex, saturnTex, uranusTex, neptuneTex;
GLuint backgroundTex;

std::vector<PlanetData> planets;
std::vector<AsteroidData> asteroids;
std::vector<MissileData> missiles;
std::vector<Particle> particles;

const int asteroidCount = 150;
const int starCount = 2000;
const float missileSpeed = 2.5f;
const float missileRange = 300.0f;
const float PI = 3.1415926535f;

// 우주선(플레이어)
glm::vec3 shipPos(0.0f, 0.0f, 40.0f);
float shipAngle = 180.0f;
float shipPitch = 0.0f;
bool keyState[256] = { false };

// 마우스 제어
float mouseSensitivity = 0.5f;
float lastMouseX = -1.0f, lastMouseY = -1.0f;
bool firstMouse = true;
bool isRightButtonDown = false;
int cameraMode = 2; // 1:1인칭, 2:전체, 3:3인칭

float globalTime = 0.0f;
// [추가] 자전/공전 제어 변수
bool isRotating = true;  // 자전 스위치 (기본 켜짐)
bool isOrbiting = true;  // 공전 스위치 (기본 켜짐)
float rotateTime = 0.0f; // 자전용 누적 시간
float orbitTime = 0.0f;  // 공전용 누적 시간



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
        float x = (rand() % 4000 - 2000) / 5.0f;
        float y = (rand() % 4000 - 2000) / 5.0f;
        float z = (rand() % 4000 - 2000) / 5.0f;

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
    // 텍스처 로딩 (파일 이름을 프로젝트 폴더에 있는 이미지 이름과 맞춰주세요!)
    sunTex = loadTexture("solar.jpg");
    mercuryTex = loadTexture("mercury.jpg"); // image_beacfa.jpg -> mercury.jpg 로 이름 바꾸세요
    venusTex = loadTexture("venus.jpg");
    earthTex = loadTexture("earth.jpg");
    moonTex = loadTexture("moon.jpg");
    marsTex = loadTexture("mercury.jpg"); // 화성 사진이 없어서 수성으로 대체함 (mars.jpg 구하면 수정)
    jupiterTex = loadTexture("jupiter.jpg");
    saturnTex = loadTexture("saturn.jpg");
    uranusTex = loadTexture("uranus.jpg");
    neptuneTex = loadTexture("neptune.jpg");
    backgroundTex = loadTexture("background.jpg");
    saturnRingTex = loadTexture("saturn_ring.png"); // 파일 이름 확인!

    planets.clear();
    // 거리, 크기, 공전, 자전, 텍스처
    planets.push_back({ 10.0f, 0.3f, 4.0f, 1.0f, mercuryTex });
    planets.push_back({ 15.0f, 0.5f, 2.5f, 0.8f, venusTex });
    planets.push_back({ 20.0f, 0.5f, 1.5f, 2.0f, earthTex });
    planets.push_back({ 25.0f, 0.4f, 1.2f, 1.8f, marsTex });
    planets.push_back({ 35.0f, 1.5f, 0.6f, 4.0f, jupiterTex });
    planets.push_back({ 45.0f, 1.3f, 0.4f, 3.8f, saturnTex });
    planets.push_back({ 55.0f, 1.0f, 0.3f, 3.0f, uranusTex });
    planets.push_back({ 65.0f, 1.0f, 0.2f, 3.2f, neptuneTex });
}

void initAsteroids() {
    asteroids.clear();
    for (int i = 0; i < asteroidCount; ++i) {
        AsteroidData a;
        float range = 150.0f;
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
    if (keyState['q']) moveDir += up;
    if (keyState['e']) moveDir -= up;

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
}

void Mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
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
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 300.0f);

        float winY = (float)viewport[3] - (float)y;
        glm::vec3 nearPt = glm::unProject(glm::vec3(x, winY, 0), view, proj, glm::vec4(viewport[0], viewport[1], viewport[2], viewport[3]));
        glm::vec3 farPt = glm::unProject(glm::vec3(x, winY, 1), view, proj, glm::vec4(viewport[0], viewport[1], viewport[2], viewport[3]));
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

        if (!hit) targetPos = rayOrigin + rayDir * 300.0f;

        MissileData m;
        glm::vec3 fireDir = glm::normalize(targetPos - shipPos);
        m.position = shipPos + fireDir * 2.0f;
        m.velocity = fireDir * missileSpeed;
        m.angle = glm::degrees(atan2(fireDir.x, fireDir.z));
        m.lifeTime = 0.0f; m.active = true;
        missiles.push_back(m);
    }

    if (button == GLUT_RIGHT_BUTTON) {
        if (state == GLUT_DOWN) {
            isRightButtonDown = true;
            lastMouseX = (float)x; lastMouseY = (float)y;
        }
        else if (state == GLUT_UP) {
            isRightButtonDown = false;
        }
    }
}

void Motion(int x, int y) {
    if (!isRightButtonDown) return;
    if (firstMouse) { lastMouseX = x; lastMouseY = y; firstMouse = false; return; }
    float xoffset = x - lastMouseX;
    float yoffset = y - lastMouseY;
    lastMouseX = x; lastMouseY = y;

    shipAngle -= xoffset * mouseSensitivity;
    shipPitch -= yoffset * mouseSensitivity;
    if (shipPitch > 89.0f) shipPitch = 89.0f;
    if (shipPitch < -89.0f) shipPitch = -89.0f;
    glutPostRedisplay();
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
    else if (cameraMode == 2) { cameraPos = glm::vec3(0, 100, 10); cameraTarget = glm::vec3(0, 0, 0); cameraUp = glm::vec3(0, 0, -1); }
    else { cameraPos = shipPos - (shipDir * 8.0f) + glm::vec3(0, 4, 0); cameraTarget = shipPos + shipDir * 5.0f; }

    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 2000.0f);

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


    // 태양
    glm::mat4 sunModel = glm::rotate(glm::mat4(1.0f), glm::radians(rotateTime * 0.2f), glm::vec3(0, 1, 0));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::scale(sunModel, glm::vec3(4.0f))));
    glBindTexture(GL_TEXTURE_2D, sunTex);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    // 행성
    for (auto& p : planets) {
        glUniform1i(isSunLoc, 1); // 궤도선
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniform4f(colorLoc, 0.2f, 0.2f, 0.4f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for (int a = 0; a < 360; a++) glVertex3f(cos(glm::radians((float)a)) * p.distance, 0, sin(glm::radians((float)a)) * p.distance);
        glEnd();

        glUniform1i(isSunLoc, 0); // 행성 본체

        // ★★★★★ [추가된 부분 시작] ★★★★★
        // 행성 종류에 따라 대기 색상(Rim Color)을 다르게 설정합니다.
        if (p.textureID == earthTex) {
            glUniform3f(rimColorLoc, 0.2f, 0.4f, 0.8f); // 지구: 푸른 대기
        }
        else if (p.textureID == marsTex) {
            glUniform3f(rimColorLoc, 0.8f, 0.3f, 0.1f); // 화성: 붉은 대기
        }
        else if (p.textureID == venusTex) {
            glUniform3f(rimColorLoc, 0.8f, 0.6f, 0.2f); // 금성: 주황빛 대기
        }
        else {
            glUniform3f(rimColorLoc, 0.0f, 0.0f, 0.0f); // 수성, 목성 등은 끔
        }
        // ★★★★★ [추가된 부분 끝] ★★★★★

        glUniform1i(isSunLoc, 0); // 행성 본체
        glm::mat4 pModel = glm::rotate(glm::mat4(1.0f), glm::radians(globalTime * p.orbitSpeed), glm::vec3(0, 1, 0));
        pModel = glm::translate(pModel, glm::vec3(p.distance, 0, 0));
        glm::mat4 earthPos = pModel;
        pModel = glm::rotate(pModel, glm::radians(globalTime * p.selfRotateSpeed), glm::vec3(0, 1, 0));
        pModel = glm::scale(pModel, glm::vec3(p.size));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
        glUniform4f(colorLoc, 1, 1, 1, 1);
        glBindTexture(GL_TEXTURE_2D, p.textureID);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        if (p.distance == 20.0f) { // 지구일 때 달
            glm::mat4 mModel = glm::rotate(earthPos, glm::radians(globalTime * 3.0f), glm::vec3(0, 1, 0));
            mModel = glm::translate(mModel, glm::vec3(1.0f, 0, 0));
            mModel = glm::scale(mModel, glm::vec3(0.2f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mModel));
            glBindTexture(GL_TEXTURE_2D, moonTex);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
        if (p.distance == 45.0f) { // 토성 고리 ((너무 어두워서 조명 처리 안 했음))
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            // [추가] 조명 연산을 무시하고 그냥 밝게 그리기 (이 줄 추가!)
            glUniform1i(isSunLoc, 1);

            glm::mat4 rModel = glm::scale(pModel, glm::vec3(2.5f, 2.5f, 2.5f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(rModel));

            // 색상을 좀 더 밝고 화사하게 보정 (RGB를 1.0보다 높게 줘서 텍스처를 밝게 만듦)
            // 마지막 2.0f는 투명도(진하기)입니다.
            glUniform4f(colorLoc, 1.2f, 1.2f, 1.2f, 2.0f);

            glBindTexture(GL_TEXTURE_2D, saturnRingTex);
            glBindVertexArray(ringVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, (72 + 1) * 2);

            // [중요] 다음 물체(운석 등)를 위해 조명 다시 켜주기 (원상복구)
            glUniform1i(isSunLoc, 0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glBindVertexArray(sphereVAO);
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
        // 1. 미사일 기본 변환 행렬 계산
        glm::mat4 missileBase = glm::mat4(1.0f);
        missileBase = glm::translate(missileBase, m.position);
        // 진행 방향으로 회전 (초기 방향이 -Z축이라고 가정)
        missileBase = glm::rotate(missileBase, glm::radians(m.angle + 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // --- 미사일 몸체 (길쭉한 원통형) ---
        // 구를 Z축으로 길게 늘려서 원통처럼 표현
        glm::mat4 bodyModel = glm::scale(missileBase, glm::vec3(0.1f, 0.1f, 0.8f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bodyModel));
        glUniform4f(colorLoc, 0.2f, 0.8f, 1.0f, 1.0f); // 밝은 청록색 몸체
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // --- 미사일 탄두 (앞부분 뾰족하게) ---
        // 몸체 앞쪽으로 이동 후, 앞쪽을 뾰족하게 만들기 위해 Z축 스케일을 조절
        glm::mat4 headModel = glm::translate(missileBase, glm::vec3(0.0f, 0.0f, 0.8f)); // 몸체 길이만큼 앞쪽으로
        headModel = glm::scale(headModel, glm::vec3(0.1f, 0.1f, 0.3f)); // 약간 작고 길게
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(headModel));
        glUniform4f(colorLoc, 1.0f, 0.2f, 0.0f, 1.0f); // 붉은색 탄두
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // --- 미사일 날개 (뒤쪽에 십자 모양으로) ---
        for (int i = 0; i < 4; ++i) {
            glm::mat4 finModel = glm::rotate(missileBase, glm::radians(i * 90.0f), glm::vec3(0, 0, 1)); // Z축 기준 회전
            finModel = glm::translate(finModel, glm::vec3(0.15f, 0.0f, -0.6f)); // 약간 바깥쪽, 뒤쪽으로 이동
            finModel = glm::scale(finModel, glm::vec3(0.15f, 0.02f, 0.3f)); // 납작하고 길게
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(finModel));
            glUniform4f(colorLoc, 0.6f, 0.6f, 0.6f, 1.0f); // 회색 날개
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

    glutSwapBuffers();
}

void Keyboard(unsigned char key, int x, int y) {
    if (key == 27) exit(0);
    if (key >= 'A' && key <= 'Z') key = key + ('a' - 'A'); // 대소문자 보정

    keyState[key] = true;

    // [추가] 키 기능 매핑
    if (key == 'y') isRotating = !isRotating; // y 누르면 자전 토글
    if (key == 'u') isOrbiting = !isOrbiting; // u 누르면 공전 토글

    if (key == '1') cameraMode = 1;
    if (key == '2') cameraMode = 2;
    if (key == '3') cameraMode = 3;
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
    GLchar* fragmentSource = filetobuf("fragment4.glsl");
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
    globalTime += 0.5f; // 게임 전체 시간 (미사일, 이동 등은 계속 흘러야 함)

    // [추가] 스위치가 켜져있을 때만 시간 누적
    if (isRotating) rotateTime += 0.5f;
    if (isOrbiting) orbitTime += 0.5f;
    updateMovement();
    updateGameLogic();
    updateParticles();
    glutPostRedisplay();
    glutTimerFunc(16, Timer, 0);
}

// -------------------------------------------------------------
// [수정된 Main 함수] void -> int로 변경하고 return 0 추가
// -------------------------------------------------------------
int main(int argc, char** argv)
{
    width = 1200;
    height = 800;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(50, 50);
    glutInitWindowSize(width, height);
    glutCreateWindow("Solar System Adventure");

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
    isSunLoc = glGetUniformLocation(shaderProgramID, "isSun");
    rimColorLoc = glGetUniformLocation(shaderProgramID, "rimColor"); // [추가]

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
    glutMotionFunc(Motion);
    glutTimerFunc(16, Timer, 0);

    glutMainLoop();

    return 0; // return 0 필수!
}