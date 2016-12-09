#version 330

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

out vec3 Normal;
out vec3 FragPos;

uniform mat4 u_M;
uniform mat4 u_MVP;

void main()
{
    gl_Position = u_MVP * vec4(position, 1.0f);
    FragPos = vec3(u_M * vec4(position, 1.0f));
    Normal = normal;
} 
