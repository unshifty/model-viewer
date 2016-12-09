#version 330

in vec3 Position_ws;
in vec3 Normal_cs;
in vec3 EyeDirection_cs;
in vec3 LightDirection_cs;

out vec3 color;

uniform mat4 MV;
uniform vec3 lightPosition_ws;

void main(void)
{
	vec3 LightColor = vec3(0.8,0.8,0.8);
	float LightPower = 50.0f;

	// Distance to the light
	float distance = length(lightPosition_ws - Position_ws);
	// Normal of the computed fragment, in camera space
	vec3 n = normalize(Normal_cs);
	// Direction of the light (from the fragment to the light)
	vec3 l = normalize(LightDirection_cs);
	// Cosine of the angle between the normal and the light direction, 
	// clamped above 0
	//  - light is at the vertical of the triangle -> 1
	//  - light is perpendicular to the triangle -> 0
	//  - light is behind the triangle -> 0
	float cosTheta = clamp(dot( n,l ), 0, 1);

	// Eye vector (towards the camera)
	vec3 E = normalize(EyeDirection_cs);
	// Direction in which the triangle reflects the light
	vec3 R = reflect(-l, n);
	// Cosine of the angle between the Eye vector and the Reflect vector,
	// clamped to 0
	//  - Looking into the reflection -> 1
	//  - Looking elsewhere -> < 1
	float cosAlpha = clamp(dot(E, R), 0, 1);

    color = LightColor * LightPower * cosTheta / (distance * distance) +
			LightColor * LightPower * pow(cosAlpha, 5) / (distance * distance);
}