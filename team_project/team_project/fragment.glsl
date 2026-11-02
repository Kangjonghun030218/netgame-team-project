#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 lightPos; 
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform int isSun;

void main()
{
    // 태양은 조명 영향 X (그냥 발광체)
    if(isSun == 1) {
        FragColor = vec4(objectColor, 1.0);
        return;
    }

    // Ambient (약한 전역광)
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse (빛과 표면의 각도)
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (빛이 닿는 면에서만 반사 발생)
    float specularStrength = 0.5;
    float spec = 0.0;

    if(diff > 0.0) { 
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    }

    vec3 specular = specularStrength * spec * lightColor;

    // 최종 조명 결과
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
