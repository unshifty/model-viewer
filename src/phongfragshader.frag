#version 330
 
in vec3 Normal;  
in vec3 FragPos;  

out vec4 color;
  
uniform vec3 u_lightPos; 
uniform vec3 u_lightColor;
uniform vec3 u_objectColor;

void main()
{
    // Ambient
    float ambientStrength = 0.1f;
    vec3 ambient = ambientStrength * u_lightColor;

    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(u_lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_lightColor;

    vec3 result = (ambient + diffuse) * u_objectColor;
    color = vec4(result, 1.0f);
} 