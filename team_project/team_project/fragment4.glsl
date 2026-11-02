#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec4 objectColor;
uniform bool isSun;
uniform sampler2D texture1;

// [추가] 대기권 색상 (기본값은 검은색으로 줘서 빛나지 않게)
uniform vec3 rimColor; 

void main()
{
    vec4 texColor = texture(texture1, TexCoord);
    vec3 color = texColor.rgb * objectColor.rgb;

    // 태양이나 파티클은 라이팅/대기 효과 무시
    if(isSun) {
        FragColor = vec4(color, texColor.a * objectColor.a);
        return;
    }

    // --- 기본 라이팅 ---
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
    vec3 specular = specularStrength * spec * vec3(1.0);

    // ----------------------------------------------------
    // [추가] 림 라이팅 (Rim Lighting) - 대기 효과 구현
    // ----------------------------------------------------
    // 1. 시선과 법선이 수직이 될수록(가장자리일수록) 값이 1.0에 가까워짐
    float rimFactor = 1.0 - max(dot(viewDir, norm), 0.0);
    
    // 2. pow(..., 3.0)을 해서 가장자리에만 얇게 맺히도록 조절 (숫자가 클수록 얇아짐)
    rimFactor = smoothstep(0.0, 1.0, pow(rimFactor, 3.0));
    
    // 3. 설정한 대기권 색상과 섞음
    vec3 rim = rimFactor * rimColor * 2.0; // 2.0을 곱해서 좀 더 밝게 부스팅

    // 최종 결과에 rim 더하기
    vec3 result = ambient + diffuse + specular + rim;
    
    // ----------------------------------------------------

    FragColor = vec4(result, objectColor.a);
}