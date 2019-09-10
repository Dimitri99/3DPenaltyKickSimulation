#version 330 core 

in vec3 lightDir;
in vec3 fragNor;
in vec3 dif;
in vec3 amb;

out vec3 color;

void main()
{
	vec3 LD = normalize(lightDir);
	
	float brightness = max(0, dot(LD, fragNor));
	color = (brightness * vec3(1.0, 1.0, 1.0) * dif) + (amb * vec3(1, 1, 1));
}