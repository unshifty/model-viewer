#version 330

in vec3 vertPosition_ms;
in vec3 normal_ms;

out vec3 Position_ws;
out vec3 Normal_cs;
out vec3 EyeDirection_cs;
out vec3 LightDirection_cs;

uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform vec3 lightPosition_ws;

void main()
{
	vec3 lightDir;
	float intensity;

    gl_Position = MVP * vec4(vertPosition_ms, 1);

	Position_ws = (M * vec4(vertPosition_ms, 1)).xyz;

	vec3 vertPosition_cs = (V * M * vec4(vertPosition_ms, 1)).xyz;
	EyeDirection_cs = vec3(0,0,0) - vertPosition_cs;

	vec3 lightPosition_cs = (V * vec4(lightPosition_ws, 1)).xyz;
	LightDirection_cs = lightPosition_cs + EyeDirection_cs;

	Normal_cs = (V * M * vec4(normal_ms, 0)).xyz;
}
