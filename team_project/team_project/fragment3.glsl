#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord; // 텍스처 좌표 받기

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec4 objectColor; // 기존 색상 (틴트 효과나 파티클용)
uniform bool isSun;

uniform sampler2D texture1; // [추가] 이미지 텍스처

void main()
{
    // 텍스처에서 색상 가져오기
    vec4 texColor = texture(texture1, TexCoord);
    
    // 만약 텍스처가 없는 물체(파티클 등)를 위해 objectColor와 섞어줌
    // (텍스처가 흰색이면 objectColor가 나오고, 텍스처가 있으면 텍스처 색이 나옴)
    vec3 color = texColor.rgb * objectColor.rgb; 

    // 태양이나 파티클(자체 발광)은 라이팅 무시
    if(isSun) {
        FragColor = vec4(color, texColor.a * objectColor.a);
        return;
    }

    // --- 라이팅 계산 ---
    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * color;
  
    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color;

    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0); // 반사광은 흰색

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, objectColor.a); // 투명도는 유지
}