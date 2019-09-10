#version 330 core

uniform sampler2D Texture;

in vec2 vTexCoord;
in float dCo;

out vec4 color;

void main() {
	vec4 texColor = texture(Texture, vTexCoord);
	
	// add a little ambient light to the texture
	if (texColor.x > .9 && texColor.y > .9 && texColor.z > .9)
		color = dCo * texColor + vec4(.4, .4, .4, 0);
	else
		color = dCo * texColor;
}