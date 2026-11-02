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
    // 화면 출력 위치
    gl_Position = projection * view * model * vec4(vPos, 1.0);

    // 월드 좌표 (빛 계산용)
    FragPos = vec3(model * vec4(vPos, 1.0));

    // 법선 변환
    Normal = mat3(transpose(inverse(model))) * vNormal;
}
