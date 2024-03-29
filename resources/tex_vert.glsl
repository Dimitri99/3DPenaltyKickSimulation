#version 330 core

layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;
uniform mat4 P;
uniform mat4 M;
uniform mat4 V;
uniform vec3 lightPos;

out float dCo;
out vec2 vTexCoord;
out vec3 fragNor;

void main() {

  /* First model transforms */
  gl_Position = P * V * M * vec4(vertPos.xyz, 1.0);

  fragNor = normalize((M * vec4(vertNor, 0.0)).xyz);
  vec3 lightDir = normalize(lightPos - (M * vec4(vertPos, 0.0)).xyz);

  /* diffuse coefficient for a directional light */
  dCo = max(0, dot(lightDir, fragNor));

  /* pass through the texture coordinates to be interpolated */
  vTexCoord = vertTex;
}
