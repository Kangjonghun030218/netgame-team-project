#version 330 core

layout (location = 0) in vec3 aPos; // 정점 위치

out vec3 FragPos; // 프래그먼트의 월드 좌표
out vec3 Normal;  // 법선 벡터

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 현재 정점의 월드 좌표 계산
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // 법선 벡터 계산 (구체이므로 위치 벡터를 회전/크기변환 하여 사용)
    // model 행렬의 역행렬의 전치행렬을 사용하여 비균등 크기 변환 시 법선 깨짐 방지
    Normal = mat3(transpose(inverse(model))) * aPos;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}