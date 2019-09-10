#version 330 core 

in vec3 lightDir;
in vec3 fragNor;
in vec4 view;
in vec4 viewVec;
in vec3 dif;
in vec3 amb;
in vec3 spec;
in float sh;

vec3 halfVec;
vec3 interpNor;

out vec4 color;

void main()
{
	vec3 LD = normalize(lightDir);
	interpNor = normalize(fragNor);

	float brightness = max(0, dot(LD, interpNor));
	// calculates total diffusion
	vec3 totDiff = (brightness * vec3(1.0, 1.0, 1.0) * dif);

	vec3 totAmb = (amb * vec3(1.0, 1.0, 1.0));

	// finds angle of specular lighting
	halfVec = normalize(LD + normalize(view).xyz);
	float angle = max(dot(interpNor, halfVec), 0);
	
	// computes specular lighting
	vec3 totSpec = pow(angle, sh) * spec * vec3(1, 1, 1);

	// Adds all colors together
	color = vec4(totDiff + totAmb + totSpec, 1.0);
}