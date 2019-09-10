#version 330 core
layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec3 vertNor;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

uniform vec3 MatDif;
uniform vec3 MatAmb;
uniform vec3 MatSpec;
uniform float shine;
uniform vec3 lightPos;

out vec3 lightDir;
out vec3 fragNor;
out vec3 dif;
out vec3 amb;
out vec3 spec;
out float sh;
out vec4 view;


void main()
{
	gl_Position = P * V * M * vertPos;

	// recalculate the normal based on transformation of object
	fragNor = normalize((M * vec4(vertNor, 0.0)).xyz);

	// calculate light direction
	lightDir = lightPos - (M * vertPos).xyz;
	// since position is at vec3(0, 0, 0), it is not included in calculation of half vector
	view = -1.0 * (vertPos);

	dif = MatDif;
	amb = MatAmb;
	spec = MatSpec;
	sh = shine;
}