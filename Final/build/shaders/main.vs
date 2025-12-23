#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform mat3 normalMatrix;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec4 LightSpacePos;
    vec3 Color;
} vs_out;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = normalize(normalMatrix * aNormal);
    vs_out.TexCoord = aTex;
    vs_out.Color = aColor;
    vs_out.LightSpacePos = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}
