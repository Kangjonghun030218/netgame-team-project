#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- 함수 선언 ---
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
GLvoid drawScene();
GLvoid Reshape(int w, int h);
void Mouse(int button, int state, int x, int y);
void Keyboard(unsigned char key, int x, int y);
void KeyboardUp(unsigned char key, int x, int y);
void Motion(int x, int y);
void Timer(int value);
void initSphereData();
void initPlanetSettings();

// --- 구조체 정의 ---
struct PlanetData {
    float distance;
    float size;
    float orbitSpeed;
    float selfRotateSpeed;
    glm::vec3 color;
};

// [수정됨] 운석 구조체: 체력(HP) 추가
struct AsteroidData {
    glm::vec3 position;      // 현재 위치
    glm::vec3 velocity;      // 이동 속도
    float size;              // 현재 크기
    float initialSize;       // 초기 크기 (파괴 효과 계산용)

    float rotationSpeed;     // 제자리 회전 속도
    glm::vec3 rotationAxis;  // 제자리 회전 축

    int health;              // [추가] 체력 (몇 대 맞으면 부서지는지)
};

// [추가됨] 미사일 구조체
struct MissileData {
    glm::vec3 position; // 위치
    glm::vec3 velocity; // 속도 벡터
    float angle;        // 미사일이 바라보는 방향 (우주선 방향과 동일)
    float lifeTime;     // 수명 (너무 오래 날아가면 삭제)
    bool active;        // 충돌 시 false가 됨
};

// [추가됨] 파티클(입자) 구조체
struct Particle {
    glm::vec3 position; // 위치
    glm::vec3 velocity; // 날아가는 방향/속도
    glm::vec3 color;    // 색상
    float life;         // 수명 (1.0에서 시작해서 0.0이 되면 사라짐)
    float size;         // 크기
    float fadeSpeed;    // 사라지는 속도
};
// [추가됨] 파티클 목록
std::vector<Particle> particles;

GLuint asteroidVAO, asteroidVBO; // 운석 모델 VAO/VBO
std::vector<AsteroidData> asteroids; // 운석들 정보 저장
const int asteroidCount = 150; // 만들 운석 개수 (원하는 만큼 조절 가능)
// [추가됨] 미사일 관련 전역 변수
std::vector<MissileData> missiles; // 발사된 미사일 목록 저장
const float missileSpeed = 2.5f;   // 미사일 속도
const float missileRange = 200.0f; // 이 거리를 넘어가면 삭제

// --- 전역 변수 ---

// [수정] FPS 카메라 제어 변수
float shipPitch = 0.0f;        // 위아래 보는 각도
float mouseSensitivity = 0.5f; // 마우스 감도

// [추가] 마우스 이동 계산을 위한 변수
float lastMouseX = -1.0f; // 초기화 값
float lastMouseY = -1.0f;
bool firstMouse = true;   // 프로그램 시작 후 첫 마우스 움직임 감지용

bool isRightButtonDown = false;

// --- 전역 변수 ---
GLint width, height;
GLuint shaderProgramID;
GLuint vertexShader, fragmentShader;
GLuint modelLoc, viewLoc, projLoc, colorLoc;
GLuint lightPosLoc;
GLuint viewPosLoc;
GLuint isSunLoc;
GLuint sphereVAO, sphereVBO, sphereEBO;
GLuint starVAO, starVBO; // 별을 그리기 위한 버퍼
const int starCount = 2000; // 별 개수
int sphereIndexCount = 0;

// 우주선(플레이어)
// 우주선(플레이어)
glm::vec3 shipPos(0.0f, 0.0f, 40.0f); // 초기 위치
float shipAngle = 180.0f;             // Y축 회전 각도 (태양 쪽을 바라보게 수정)
float shipSpeed = 0.0f;               // 현재 속도 (관성 적용을 위해)

// 카메라 모드
int cameraMode = 2; // 2번(전체 뷰)로 시작

// 애니메이션
float globalTime = 0.0f;
const float PI = 3.1415926535f;

// 키보드 상태 저장 (동시 입력 및 부드러운 이동을 위해)
bool keyState[256] = { false };

// 물리 변수 (부드러운 움직임 핵심)
glm::vec3 shipVelocity(0.0f, 0.0f, 0.0f); // 현재 속도 벡터
float acceleration = 0.05f; // 가속도 (엑셀 밟을 때 힘)
float friction = 0.96f;     // 마찰력 (1.0에 가까울수록 잘 안 멈춤 = 얼음판 느낌)
float maxSpeed = 2.0f;      // 최대 속도 제한



std::vector<PlanetData> planets;

char* filetobuf(const char* file)
{
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

void initStars() {
    std::vector<float> stars;
    for (int i = 0; i < starCount; ++i) {
        // -150 ~ 150 범위의 아주 넓은 공간에 랜덤하게 뿌림
        float x = (rand() % 3000 - 1500) / 10.0f;
        float y = (rand() % 3000 - 1500) / 10.0f;
        float z = (rand() % 3000 - 1500) / 10.0f;

        // 너무 가까이 있는 별은 제외 (우주선 뚫고 지나가는 것 방지)
        if (abs(x) < 50 && abs(y) < 50 && abs(z) < 50) continue;

        stars.push_back(x);
        stars.push_back(y);
        stars.push_back(z);
    }

    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(float), &stars[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// 울퉁불퉁한 운석 모델 데이터 생성 함수
void initAsteroidModel() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const int stackCount = 20; // 운석은 좀 거칠어도 되므로 20 정도로 설정
    const int sectorCount = 20;
    float baseRadius = 1.0f;

    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI / 2 - i * PI / stackCount;
        float xy = baseRadius * cosf(stackAngle);
        float z = baseRadius * sinf(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * 2 * PI / sectorCount;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // ▼▼▼ [핵심] 노이즈를 더해 울퉁불퉁하게 만듦 ▼▼▼
            // 0.8 ~ 1.2 사이의 랜덤한 값을 곱함
            float noise = 0.8f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / 0.4f));
            x *= noise;
            y *= noise;
            z *= noise;
            // ▲▲▲ [핵심] 끝 ▲▲▲

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }

    // 인덱스 생성 (initSphereData와 동일)
    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;
        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1); indices.push_back(k2); indices.push_back(k1 + 1);
            }
            if (i != (stackCount - 1)) {
                indices.push_back(k1 + 1); indices.push_back(k2); indices.push_back(k2 + 1);
            }
        }
    }

    // VAO/VBO 생성 및 바인딩
    glGenVertexArrays(1, &asteroidVAO);
    glGenBuffers(1, &asteroidVBO);
    // EBO는 sphereEBO를 재활용해도 됩니다 (인덱스 구조가 같으므로)

    glBindVertexArray(asteroidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, asteroidVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    // 인덱스 버퍼는 기존 sphereEBO를 재사용 (주의: initSphereData가 먼저 호출되어야 함)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void initAsteroids() {
    asteroids.clear();

    for (int i = 0; i < asteroidCount; ++i) {
        AsteroidData a;

        // 위치, 속도 설정 (기존과 동일)
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

        // [수정] 크기 및 체력 설정
        // 크기가 클수록 체력도 높게 설정 (예: 크기 0.5~1.5, 체력 2~5)
        float randomFactor = static_cast<float>(rand()) / RAND_MAX; // 0.0 ~ 1.0
        a.initialSize = 0.5f + randomFactor * 1.0f;
        a.size = a.initialSize; // 현재 크기 초기화
        a.health = 2 + static_cast<int>(randomFactor * 3.0f); // 2~5 사이의 체력

        // 회전 설정 (기존과 동일)
        a.rotationSpeed = (static_cast<float>(rand()) / RAND_MAX * 4.0f) - 2.0f;
        a.rotationAxis = glm::normalize(glm::vec3(
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX
        ));

        asteroids.push_back(a);
    }

    initAsteroidModel();
}

void initPlanetSettings() {
    // 수금지화목토천해 데이터
    // 포맷: { 거리, 크기, 공전속도, 자전속도, 색상 }
    planets.push_back({ 10.0f, 0.3f,  4.0f,   1.0f,   glm::vec3(0.7f, 0.7f, 0.7f) }); // 수성 (거리 5 -> 10)
    planets.push_back({ 15.0f, 0.5f,  2.5f,   0.8f,   glm::vec3(0.9f, 0.8f, 0.5f) }); // 금성 (거리 7 -> 15)
    planets.push_back({ 20.0f, 0.5f,  1.5f,   2.0f,   glm::vec3(0.0f, 0.4f, 1.0f) }); // 지구 (거리 9.5 -> 20)
    planets.push_back({ 25.0f, 0.4f,  1.2f,   1.8f,   glm::vec3(1.0f, 0.2f, 0.0f) }); // 화성 (거리 12 -> 25)

    // 목성부터는 거리를 좀 더 확 벌려서 외행성 느낌을 줍니다.
    planets.push_back({ 35.0f, 1.5f,  0.6f,   4.0f,   glm::vec3(0.8f, 0.6f, 0.4f) }); // 목성 (거리 16 -> 35)
    planets.push_back({ 45.0f, 1.3f,  0.4f,   3.8f,   glm::vec3(0.9f, 0.8f, 0.6f) }); // 토성 (거리 21 -> 45)
    planets.push_back({ 55.0f, 1.0f,  0.3f,   3.0f,   glm::vec3(0.4f, 0.9f, 0.9f) }); // 천왕성 (거리 25 -> 55)
    planets.push_back({ 65.0f, 1.0f,  0.2f,   3.2f,   glm::vec3(0.1f, 0.1f, 0.8f) }); // 해왕성 (거리 29 -> 65)
}

void initSphereData() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const int stackCount = 72;
    const int sectorCount = 72;
    float radius = 1.0f;

    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI / 2 - i * PI / stackCount;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * 2 * PI / sectorCount;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }

    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;
        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stackCount - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
    sphereIndexCount = indices.size();

    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// [추가됨] 폭발 이펙트 생성 (운석 파괴 시)
void spawnExplosion(glm::vec3 pos, glm::vec3 color, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = pos;

        // 사방으로 랜덤하게 튀는 속도
        p.velocity = glm::vec3(
            (rand() % 100 - 50) / 100.0f,
            (rand() % 100 - 50) / 100.0f,
            (rand() % 100 - 50) / 100.0f
        );
        p.velocity = glm::normalize(p.velocity) * (0.2f + (rand() % 100 / 100.0f) * 0.3f); // 속도 랜덤

        p.color = color; // 운석 색상 따라감
        p.life = 1.0f;
        p.size = 0.2f + (rand() % 100 / 100.0f) * 0.3f; // 크기 랜덤
        p.fadeSpeed = 0.02f + (rand() % 100 / 100.0f) * 0.02f; // 사라지는 속도 랜덤

        particles.push_back(p);
    }
}

// [수정됨] 엔진 트레일 생성 (색상 선택 가능)
void spawnTrail(glm::vec3 pos, glm::vec3 color) {
    Particle p;
    p.position = pos;

    // 뒤로 퍼지는 느낌
    p.velocity = glm::vec3(
        (rand() % 100 - 50) / 400.0f,
        (rand() % 100 - 50) / 400.0f,
        (rand() % 100 - 50) / 400.0f
    );

    p.color = color; // [수정] 받아온 색상을 적용
    p.life = 1.0f;
    p.size = 0.3f;
    p.fadeSpeed = 0.05f;

    particles.push_back(p);
}
// [추가됨] 파티클 상태 업데이트
void updateParticles() {
    for (int i = 0; i < particles.size(); ) {
        // 이동
        particles[i].position += particles[i].velocity;

        // 수명 감소 (투명해짐)
        particles[i].life -= particles[i].fadeSpeed;

        // 크기도 점점 작아짐
        particles[i].size *= 0.95f;

        // 수명이 다하면 삭제
        if (particles[i].life <= 0.0f) {
            particles.erase(particles.begin() + i);
        }
        else {
            ++i;
        }
    }
}

void main(int argc, char** argv)
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

    initSphereData(); // 이게 가장 먼저 호출되어야 sphereEBO가 생성됨
    initPlanetSettings();
    initStars();

    initAsteroids(); // <-- [추가] 여기!

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutKeyboardUpFunc(KeyboardUp); // <--- [추가] 키 떼기 콜백 등록!
    glutMouseFunc(Mouse);
    glutMotionFunc(Motion);
    glutTimerFunc(16, Timer, 0);

    glutMainLoop();
}

void make_vertexShaders()
{
    GLchar* vertexSource = filetobuf("vertex2.glsl");
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    GLint result;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
    if (!result) { std::cerr << "Vertex Shader Error\n"; }
    free(vertexSource);
}

void make_fragmentShaders()
{
    GLchar* fragmentSource = filetobuf("fragment2.glsl");
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    GLint result;
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
    if (!result) { std::cerr << "Fragment Shader Error\n"; }
    free(fragmentSource);
}

GLuint make_shaderProgram()
{
    int result;
    GLuint shaderID = glCreateProgram();
    glAttachShader(shaderID, vertexShader);
    glAttachShader(shaderID, fragmentShader);
    glLinkProgram(shaderID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
    if (!result) { std::cerr << "Shader Program Error\n"; }
    glUseProgram(shaderID);
    return shaderID;
}

GLvoid drawScene()
{
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    glm::mat4 view = glm::mat4(1.0f);
    glm::vec3 cameraPos, cameraTarget, cameraUp(0.0f, 1.0f, 0.0f);

    glm::vec3 shipDir;
    shipDir.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    shipDir.y = sin(glm::radians(shipPitch));
    shipDir.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    shipDir = glm::normalize(shipDir);
    glm::vec3 shipRight = glm::normalize(glm::cross(shipDir, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (cameraMode == 1) {
        // 1인칭: 이제 마우스가 보는 곳을 정확히 바라봄
        cameraPos = shipPos + glm::vec3(0.0f, 0.5f, 0.0f) + (shipDir * 2.0f);

        cameraTarget = cameraPos + shipDir;
    }
    else if (cameraMode == 2) {
        cameraPos = glm::vec3(0.0f, 100.0f, 10.0f);
        cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
        cameraUp = glm::vec3(0.0f, 0.0f, -1.0f);
        view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    }
    else if (cameraMode == 3) {
        cameraPos = shipPos - (shipDir * 8.0f) + glm::vec3(0.0f, 4.0f, 0.0f);
        cameraTarget = shipPos + (shipDir * 5.0f);
    }

    if (cameraMode != 2) view = glm::lookAt(cameraPos, cameraTarget, cameraUp);

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 300.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // --- 별 배경 ---
    glUniform1i(isSunLoc, 1);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    // [수정] 4번째 인자 1.0f (불투명)
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    glBindVertexArray(starVAO);
    glPointSize(1.5f);
    glDrawArrays(GL_POINTS, 0, starCount);

    glUniform3f(lightPosLoc, 0.0f, 0.0f, 0.0f);
    glUniform3f(viewPosLoc, cameraPos.x, cameraPos.y, cameraPos.z);

    glBindVertexArray(sphereVAO);

    // ================== 태양 그리기 ==================
    glUniform1i(isSunLoc, 1);

    glm::mat4 sunModel = glm::mat4(1.0f);
    sunModel = glm::rotate(sunModel, glm::radians(globalTime * 0.2f), glm::vec3(0.0f, 1.0f, 0.0f));
    sunModel = glm::scale(sunModel, glm::vec3(4.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sunModel));

    // [수정] 태양 본체: 밝은 노랑, 불투명(1.0)
    glUniform4f(colorLoc, 1.0f, 0.9f, 0.0f, 1.0f);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    // ================== 태양 후광 효과 (핵심) ==================
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glm::mat4 glowModel = sunModel;
    glowModel = glm::scale(glowModel, glm::vec3(1.2f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glowModel));

    // ▼▼▼ [핵심 수정] 4번째 인자가 0.3f (30%만 보이고 70%는 투명함) ▼▼▼
    glUniform4f(colorLoc, 1.0f, 0.6f, 0.1f, 0.3f);

    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ================== 행성 그리기 ==================
    for (int i = 0; i < planets.size(); i++) {
        PlanetData& p = planets[i];

        // 궤도 선
        glUniform1i(isSunLoc, 1);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        // [수정] 궤도 선 불투명
        glUniform4f(colorLoc, 0.2f, 0.2f, 0.4f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for (int a = 0; a < 360; a++) {
            float rad = glm::radians((float)a);
            glVertex3f(cos(rad) * p.distance, 0.0f, sin(rad) * p.distance);
        }
        glEnd();

        // 행성
        glUniform1i(isSunLoc, 0);
        glm::mat4 pModel = glm::mat4(1.0f);
        pModel = glm::rotate(pModel, glm::radians(globalTime * p.orbitSpeed), glm::vec3(0.0f, 1.0f, 0.0f));
        pModel = glm::translate(pModel, glm::vec3(p.distance, 0.0f, 0.0f));
        glm::mat4 earthModel = pModel;
        pModel = glm::rotate(pModel, glm::radians(globalTime * p.selfRotateSpeed), glm::vec3(0.0f, 1.0f, 0.0f));
        pModel = glm::scale(pModel, glm::vec3(p.size));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
        // [수정] 행성 색상 불투명
        glUniform4f(colorLoc, p.color.x, p.color.y, p.color.z, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // 달
        if (i == 2) {
            glm::mat4 moonModel = earthModel;
            moonModel = glm::rotate(moonModel, glm::radians(globalTime * 3.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            moonModel = glm::translate(moonModel, glm::vec3(1.0f, 0.0f, 0.0f));
            moonModel = glm::scale(moonModel, glm::vec3(0.2f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(moonModel));
            glUniform4f(colorLoc, 0.8f, 0.8f, 0.8f, 1.0f);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }

        // 토성 고리
        if (i == 5) {
            glm::mat4 ringModel = pModel;
            ringModel = glm::scale(ringModel, glm::vec3(2.5f, 0.05f, 2.5f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(ringModel));
            glUniform4f(colorLoc, 0.8f, 0.7f, 0.5f, 1.0f);
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }
    }

    // ================== 운석(Asteroid) 그리기 [파괴 효과 적용] ==================
    glBindVertexArray(asteroidVAO);
    glUniform1i(isSunLoc, 0);

    for (const auto& a : asteroids) {
        // 크기가 0 이하면 그리지 않음
        if (a.size <= 0.001f) continue;

        glm::mat4 aModel = glm::mat4(1.0f);
        // 위치 이동 (Drift)
        glm::vec3 currentPos = a.position + (a.velocity * globalTime);
        aModel = glm::translate(aModel, currentPos);
        // 자전
        aModel = glm::rotate(aModel, glm::radians(globalTime * a.rotationSpeed), a.rotationAxis);
        // 크기
        aModel = glm::scale(aModel, glm::vec3(a.size));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(aModel));

        // ▼▼▼ [파괴 효과 색상] 체력이 깎일수록 붉은색으로 변함 ▼▼▼
        // 초기 크기 대비 현재 크기 비율 계산
        float damageRatio = 1.0f - (a.size / a.initialSize);
        // 기본 회갈색(0.4, 0.35, 0.3)에 데미지 비율만큼 붉은색(1.0, 0.0, 0.0)을 섞음
        glm::vec3 damagedColor = glm::mix(glm::vec3(0.4f, 0.35f, 0.3f), glm::vec3(0.8f, 0.1f, 0.0f), damageRatio * 1.5f);

        glUniform4f(colorLoc, damagedColor.r, damagedColor.g, damagedColor.b, 1.0f);
        // ▲▲▲ 색상 적용 끝 ▲▲▲

        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }


    // ================== [추가됨] 미사일 그리기 (복합 모델) ==================
    // 매끈한 구체 VAO 재사용
    glBindVertexArray(sphereVAO);
    glUniform1i(isSunLoc, 1); // 미사일은 자체 발광 (잘 보이게)

    for (const auto& m : missiles) {
        // 1. 미사일 기본 변환 행렬 계산
        glm::mat4 missileBase = glm::mat4(1.0f);
        missileBase = glm::translate(missileBase, m.position);
        // 진행 방향으로 회전
        missileBase = glm::rotate(missileBase, glm::radians(m.angle), glm::vec3(0.0f, 1.0f, 0.0f));

        // --- 미사일 몸체 (길쭉한 원통형) ---
        glm::mat4 bodyModel = glm::scale(missileBase, glm::vec3(0.15f, 0.15f, 1.2f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bodyModel));
        // 밝은 청록색 몸체
        glUniform4f(colorLoc, 0.2f, 1.0f, 0.8f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

        // --- 미사일 탄두 (앞부분 뾰족하게) ---
        glm::mat4 headModel = glm::translate(missileBase, glm::vec3(0.0f, 0.0f, 1.1f)); // 몸체 앞쪽으로 이동
        headModel = glm::scale(headModel, glm::vec3(0.14f, 0.14f, 0.5f)); // 약간 작고 길게
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(headModel));
        // 붉은색 탄두
        glUniform4f(colorLoc, 1.0f, 0.3f, 0.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }


    // ================== 우주선 그리기 ==================
    glm::mat4 shipBaseModel = glm::mat4(1.0f);
    shipBaseModel = glm::translate(shipBaseModel, shipPos);
    shipBaseModel = glm::rotate(shipBaseModel, glm::radians(shipAngle), glm::vec3(0.0f, 1.0f, 0.0f));

    // 2. [추가] 위아래 회전 (Pitch) - 우주선이 고개를 들거나 숙임
    // X축을 기준으로 회전해야 위아래로 움직임
    shipBaseModel = glm::rotate(shipBaseModel, glm::radians(-shipPitch), glm::vec3(1.0f, 0.0f, 0.0f));

    shipBaseModel = glm::scale(shipBaseModel, glm::vec3(0.5f));

    // [수정] 모든 부품 색상 전송을 glUniform4f(..., 1.0f)로 변경
    glm::mat4 bodyModel = glm::scale(shipBaseModel, glm::vec3(0.6f, 0.6f, 3.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bodyModel));
    glUniform4f(colorLoc, 0.7f, 0.7f, 0.7f, 1.0f);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    glm::mat4 leftWingModel = glm::translate(shipBaseModel, glm::vec3(-1.2f, 0.0f, -0.5f));
    leftWingModel = glm::scale(leftWingModel, glm::vec3(1.5f, 0.1f, 1.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(leftWingModel));
    glUniform4f(colorLoc, 0.8f, 0.2f, 0.2f, 1.0f);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    glm::mat4 rightWingModel = glm::translate(shipBaseModel, glm::vec3(1.2f, 0.0f, -0.5f));
    rightWingModel = glm::scale(rightWingModel, glm::vec3(1.5f, 0.1f, 1.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(rightWingModel));
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    glm::mat4 cockpitModel = glm::translate(shipBaseModel, glm::vec3(0.0f, 0.5f, 1.0f));
    cockpitModel = glm::scale(cockpitModel, glm::vec3(0.5f, 0.4f, 0.7f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(cockpitModel));
    glUniform4f(colorLoc, 0.3f, 0.7f, 1.0f, 1.0f);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

    for (int i = -1; i <= 1; i += 2) {
        glm::mat4 engineModel = glm::translate(shipBaseModel, glm::vec3(0.5f * i, 0.0f, -2.8f));
        engineModel = glm::scale(engineModel, glm::vec3(0.4f, 0.4f, 0.6f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(engineModel));
        glUniform4f(colorLoc, 1.0f, 0.6f, 0.0f, 1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }

    // ================== [추가됨] 파티클 그리기 ==================
        // 파티클은 반투명하게 그려야 예쁨
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // GL_ONE을 쓰면 빛이 겹칠수록 밝아짐 (폭발 느낌)
    glDepthMask(GL_FALSE); // 깊이 버퍼 끄기 (다른 물체 뒤에 숨지 않고 빛처럼 보이게)

    glUniform1i(isSunLoc, 1); // 자체 발광
    glBindVertexArray(sphereVAO); // 구 모양 재활용 (작게 그림)

    for (const auto& p : particles) {
        glm::mat4 pModel = glm::mat4(1.0f);
        pModel = glm::translate(pModel, p.position);
        pModel = glm::scale(pModel, glm::vec3(p.size));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pModel));
        // 파티클 색상과 투명도(life) 적용
        glUniform4f(colorLoc, p.color.r, p.color.g, p.color.b, p.life);

        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 블렌딩 모드 원상복구
    glDisable(GL_BLEND);

    glBindVertexArray(0);
    glutSwapBuffers();
}

GLvoid Reshape(int w, int h)
{
    width = w;
    height = h;
    glViewport(0, 0, w, h);
}

void Mouse(int button, int state, int x, int y) {
    // ==========================================================
    // 1. 좌클릭: 미사일 발사 (Ray Casting + Auto Aim)
    // ==========================================================
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {

        // 1-1. 현재 화면 설정 가져오기
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

        glm::vec3 cameraPos, cameraTarget, cameraUp(0.0f, 1.0f, 0.0f);

        // [중요] drawScene과 똑같이, 현재 바라보는 방향(Pitch 포함) 계산
        glm::vec3 shipDir;
        shipDir.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
        shipDir.y = sin(glm::radians(shipPitch));
        shipDir.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
        shipDir = glm::normalize(shipDir);

        // 카메라 위치 설정 (drawScene과 동일하게 맞춰야 정확한 조준 가능)
        if (cameraMode == 1) {
            // 1인칭: 우주선 모델 안을 보지 않도록 앞으로(2.0f) 빼줌
            cameraPos = shipPos + glm::vec3(0.0f, 0.5f, 0.0f) + (shipDir * 2.0f);
            cameraTarget = cameraPos + shipDir;
        }
        else if (cameraMode == 2) {
            cameraPos = glm::vec3(0.0f, 100.0f, 10.0f);
            cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
            cameraUp = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        else if (cameraMode == 3) {
            cameraPos = shipPos - (shipDir * 8.0f) + glm::vec3(0.0f, 4.0f, 0.0f);
            cameraTarget = shipPos + (shipDir * 5.0f);
        }

        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 300.0f);

        // 1-2. 2D 마우스 좌표를 3D 레이저(Ray)로 변환
        float winX = (float)x;
        float winY = (float)viewport[3] - (float)y;

        glm::vec3 nearPoint = glm::unProject(glm::vec3(winX, winY, 0.0f), view, projection, glm::vec4(viewport[0], viewport[1], viewport[2], viewport[3]));
        glm::vec3 farPoint = glm::unProject(glm::vec3(winX, winY, 1.0f), view, projection, glm::vec4(viewport[0], viewport[1], viewport[2], viewport[3]));

        glm::vec3 rayOrigin = cameraPos;
        glm::vec3 rayDir = glm::normalize(farPoint - nearPoint);


        // 1-3. 운석 조준 판정 (Auto-Aim)
        glm::vec3 targetPos;
        bool hitAsteroid = false;
        float closestDist = 10000.0f;

        for (const auto& a : asteroids) {
            if (a.size <= 0.0f) continue; // 이미 부서진 건 패스

            // 운석의 현재 위치 계산 (Drift 반영)
            glm::vec3 aPos = a.position + (a.velocity * globalTime);

            // Ray-Sphere 충돌 계산
            glm::vec3 L = aPos - rayOrigin;
            float tca = glm::dot(L, rayDir);

            if (tca < 0) continue; // 카메라 뒤에 있는 것 무시

            float d2 = glm::dot(L, L) - tca * tca;
            float radius2 = (a.size + 1.0f) * (a.size + 1.0f); // 판정 범위 조금 넉넉하게

            if (d2 > radius2) continue; // 빗나감

            float thc = sqrt(radius2 - d2);
            float t0 = tca - thc;

            // 가장 가까운 운석 찾기
            if (t0 < closestDist) {
                closestDist = t0;
                targetPos = aPos;
                hitAsteroid = true;
            }
        }

        // 1-4. 목표 지점 확정
        if (!hitAsteroid) {
            // 아무것도 안 맞았으면 마우스 방향 저 멀리로 쏘기
            targetPos = rayOrigin + (rayDir * 300.0f);
        }

        // 1-5. 미사일 생성 및 발사
        MissileData newMissile;
        glm::vec3 fireDir = glm::normalize(targetPos - shipPos); // 우주선 -> 목표물 방향

        newMissile.position = shipPos + fireDir * 2.0f;
        newMissile.velocity = fireDir * missileSpeed;

        // 미사일 각도 (간단히 Y축 회전만 적용)
        newMissile.angle = glm::degrees(atan2(fireDir.x, fireDir.z));

        newMissile.lifeTime = 0.0f;
        newMissile.active = true;

        missiles.push_back(newMissile);
    }

    // ==========================================================
    // 2. 우클릭: 화면 회전(드래그) 모드 ON/OFF
    // ==========================================================
    if (button == GLUT_RIGHT_BUTTON) {
        if (state == GLUT_DOWN) {
            isRightButtonDown = true; // 드래그 시작 신호

            // 드래그 시작 지점을 현재 위치로 초기화 (화면 튐 방지)
            lastMouseX = (float)x;
            lastMouseY = (float)y;
        }
        else if (state == GLUT_UP) {
            isRightButtonDown = false; // 드래그 종료
        }
    }

    glutPostRedisplay();
}

// [수정됨] 우클릭 드래그 시에만 작동하는 시선 처리 함수
void Motion(int x, int y) {
    // 우클릭 상태가 아니라면 회전하지 않음 (좌클릭 드래그 등 무시)
    if (!isRightButtonDown) return;

    // 1. 이전 위치와의 차이(Delta) 계산
    float xoffset = (float)x - lastMouseX;
    float yoffset = (float)y - lastMouseY;

    // 2. 현재 위치 저장
    lastMouseX = (float)x;
    lastMouseY = (float)y;

    // 3. 각도 업데이트
    shipAngle -= xoffset * mouseSensitivity;
    shipPitch -= yoffset * mouseSensitivity;

    // 4. 각도 제한
    if (shipPitch > 89.0f) shipPitch = 89.0f;
    if (shipPitch < -89.0f) shipPitch = -89.0f;

    glutPostRedisplay();
}

void updateGameLogic() {
    // 1. 미사일 이동 업데이트 및 수명 관리
    for (int i = 0; i < missiles.size(); ) {
        if (!missiles[i].active) {
            missiles.erase(missiles.begin() + i);
            continue;
        }

        missiles[i].position += missiles[i].velocity;
        float distFromOrigin = glm::length(missiles[i].position);

        if (distFromOrigin > missileRange) {
            missiles.erase(missiles.begin() + i);
        }
        else {
            i++;
        }
    }

    // 2. 충돌 감지 (미사일 vs 운석)
    for (auto& m : missiles) {
        if (!m.active) continue;

        for (auto& a : asteroids) {
            if (a.health <= 0) continue;

            // 운석의 '현재 위치' 계산 (Drift 반영)
            glm::vec3 currentAsteroidPos = a.position + (a.velocity * globalTime);

            // 거리 계산
            float distance = glm::length(m.position - currentAsteroidPos);
            float collisionRadiusSum = a.size + 0.3f;

            if (distance < collisionRadiusSum) {
                // === 충돌 발생! ===
                m.active = false;

                a.health--;

                // ▼▼▼ [수정] a.position 대신 currentAsteroidPos를 넣어야 현재 위치에서 터짐! ▼▼▼
                spawnExplosion(currentAsteroidPos, glm::vec3(0.5f, 0.4f, 0.3f), 3);

                float healthRatio = (float)a.health / (float)(a.initialSize * 3.0f);

                if (a.health > 0)
                    a.size = a.initialSize * (0.4f + 0.6f * healthRatio);

                if (a.health <= 0) {
                    a.size = 0.0f;
                    // ▼▼▼ [수정] 여기도 currentAsteroidPos로 변경! ▼▼▼
                    spawnExplosion(currentAsteroidPos, glm::vec3(0.8f, 0.3f, 0.1f), 30);
                }
                break;
            }
        }
    }

    // 3. 파괴된 운석 제거
    asteroids.erase(
        std::remove_if(asteroids.begin(), asteroids.end(),
            [](const AsteroidData& a) { return a.size <= 0.0f; }),
        asteroids.end());
}

// 키를 누를 때: 상태를 true로 변경
void Keyboard(unsigned char key, int x, int y) {
    if (key == 27) exit(0); // ESC 종료

    // 대소문자 구분을 없애기 위해 소문자로 통일해서 저장
    if (key >= 'A' && key <= 'Z') key = key + ('a' - 'A');

    keyState[key] = true;

    // 카메라 모드 변경 같은 '즉시 반응' 키는 여기서 처리
    if (key == '1') cameraMode = 1;
    if (key == '2') cameraMode = 2;
    if (key == '3') cameraMode = 3;
}

// [추가] 키를 뗐을 때: 상태를 false로 변경
void KeyboardUp(unsigned char key, int x, int y) {
    if (key >= 'A' && key <= 'Z') key = key + ('a' - 'A');
    keyState[key] = false;
}

void updateMovement() {
    // 1. 회전 처리 (마우스 사용)

    // 2. 이동 방향 계산
    glm::vec3 moveDir(0.0f, 0.0f, 0.0f);

    glm::vec3 front;
    front.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    front.y = sin(glm::radians(shipPitch));
    front.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
    front = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, front));

    // 키 입력
    if (keyState['w']) moveDir += front;
    if (keyState['s']) moveDir -= front;
    if (keyState['a']) moveDir -= right;
    if (keyState['d']) moveDir += right;
    if (keyState['q']) moveDir += up;
    if (keyState['e']) moveDir -= up;

    // 3. 실제 이동 및 부스터 처리 [수정됨]
    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);

        // ▼▼▼ [추가] 부스터 로직 ▼▼▼
        float currentSpeed = 0.5f; // 기본 속도
        glm::vec3 engineColor = glm::vec3(0.2f, 0.6f, 1.0f); // 기본 파란색

        if (keyState['r']) { // R키를 누르고 있다면?
            currentSpeed = 1.5f; // 속도 3배 증가
            engineColor = glm::vec3(1.0f, 0.3f, 0.1f); // 붉은 주황색으로 변경
        }
        // ▲▲▲ [끝] ▲▲▲

        shipPos += moveDir * currentSpeed;

        // 엔진 위치 계산
        glm::vec3 shipBack;
        shipBack.x = sin(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
        shipBack.y = sin(glm::radians(shipPitch));
        shipBack.z = cos(glm::radians(shipAngle)) * cos(glm::radians(shipPitch));
        shipBack = glm::normalize(shipBack) * -1.5f;

        glm::vec3 engineOffset = glm::cross(glm::normalize(shipBack * -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * 0.5f;

        // [수정] spawnTrail 호출 시 결정된 색상(engineColor)을 넘겨줌
        spawnTrail(shipPos + shipBack + engineOffset, engineColor);
        spawnTrail(shipPos + shipBack - engineOffset, engineColor);
    }
}

void Timer(int value) {
    globalTime += 0.5f;

    updateMovement(); // 우주선 이동
    updateGameLogic(); // [추가] 미사일 및 충돌 로직 실행
    updateParticles(); // <-- [추가] 파티클 업데이트 실행
    glutPostRedisplay();
    glutTimerFunc(16, Timer, 0);
}