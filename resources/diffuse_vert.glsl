#version  330 core
layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec3 vertNor;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

uniform vec3 MatDif;
uniform vec3 MatAmb;
uniform vec3 lightPos;

out vec3 lightDir;
out vec3 fragNor;
out vec3 dif;
out vec3 amb;

void main()
{
	gl_Position = P * V * M * vertPos;

	// recalculate the normal based on transformation of object
	fragNor = (M * vec4(vertNor, 0.0)).xyz;

	// calculate light direction
	lightDir = lightPos - (M * vertPos).xyz;
	dif = MatDif;
	amb = MatAmb;
}