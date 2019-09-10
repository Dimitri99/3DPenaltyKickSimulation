#version 330 core 

in vec3 lightDir;
in vec3 fragNor;
in vec4 view;
in vec4 viewVec;
in vec3 dif;
in vec3 spec;
in float sh;
in vec3 amb;

vec3 halfVec;
vec3 interpNor;

out vec4 color;

void main()
{
	vec3 LD = normalize(lightDir);
	interpNor = normalize(fragNor);

	float dCo = max(0, dot(LD, interpNor));
	// calculates total diffusion
	vec3 totDiff = (dCo * vec3(1.0, 1.0, 1.0) * dif);

	vec3 totAmb = (amb * vec3(1.0, 1.0, 1.0));

	// finds angle of specular lighting
	halfVec = normalize(LD + normalize(view).xyz);
	float angle = pow(max(dot(interpNor, halfVec), 0), sh);
	
	// computes specular lighting
	vec3 totSpec = angle * spec * vec3(1, 1, 1);

	float coeff = 0.6 * dCo + 0.6 * angle;
	if (coeff > 0.9) {
 		coeff = 1.8;
 	} else if (coeff > 0.5) {
 		coeff = 0.7;
 	} else {
 		coeff = 0.4;
 	}

	// Adds all colors together
	color = vec4(coeff * (totDiff + totAmb + totSpec), 1.0);
}