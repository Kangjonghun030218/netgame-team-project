#version 330 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 1. 화면 출력 위치 (MVP 행렬 모두 사용)
    gl_Position = projection * view * model * vec4(vPos, 1.0);

    // [핵심] 조명 계산용 위치는 'World Space' 기준이어야 함!
    // 절대 여기에 'view' 행렬을 곱하면 안 됩니다.
    FragPos = vec3(model * vec4(vPos, 1.0));

    // 2. 법선 벡터 (회전/크기 적용)
    Normal = mat3(transpose(inverse(model))) * vNormal; 
}