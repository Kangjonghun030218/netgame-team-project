#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord; // [추가] 텍스처 좌표

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord; // [추가] 프래그먼트 쉐이더로 전달

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord; // 좌표 전달

    gl_Position = projection * view * vec4(FragPos, 1.0);
}