#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

// [수정] vec3 -> vec4로 변경 (마지막 숫자가 투명도)
uniform vec4 objectColor; 
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform int isSun;

void main()
{
    if(isSun == 1) {
        // 태양이나 별, 궤도 선은 조명 없이 색상 그대로 출력 (투명도 포함)
        FragColor = objectColor;
    } 
    else {
        // 일반 물체 (조명 계산)
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);
  
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
        
        float specularStrength = 0.8;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);  
        
        vec3 specular = vec3(0.0);
        if (diff > 0.0) {
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128); 
            specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);
        }
            
        // [수정] RGB 색상만 조명 계산하고, 투명도(a)는 그대로 유지
        vec3 result = (ambient + diffuse + specular) * objectColor.rgb;
       float finalAlpha = texColor.a * objectColor.a;
    
    // 알파값이 너무 낮으면(투명하면) 아예 그리지 않음 (선택사항, 깔끔한 외곽선을 위해 추천)
    if(finalAlpha < 0.1) discard; 

    FragColor = vec4(result, finalAlpha);
    }
}