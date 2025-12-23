#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec4 LightSpacePos;
    vec3 Color;
} fs_in;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

uniform Material material;
uniform Light light;
uniform vec3 eyePos;
uniform sampler2D diffuseMap;
uniform sampler2D shadowMap;
uniform int useTexture;
uniform vec2 uvScale;
uniform float ambientBoost;
uniform vec3 fillLight;
uniform int lightEnabled;
uniform float roomAmbient;

float ShadowCalculation(vec4 lightSpacePos, vec3 normal, vec3 lightDir) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) {
        return 0.0;
    }
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias) > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec3 norm = normalize(fs_in.Normal);
    vec3 lightDir = normalize(light.position - fs_in.FragPos);

    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    if (lightEnabled == 0) {
        intensity = 0.0;
    }

    vec3 baseColor = fs_in.Color;
    if (useTexture == 1) {
        baseColor = texture(diffuseMap, fs_in.TexCoord * uvScale).rgb;
    }

    vec3 ambient = roomAmbient * baseColor + light.ambient * material.ambient * baseColor + ambientBoost * baseColor + fillLight * baseColor;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse * baseColor);

    vec3 viewDir = normalize(eyePos - fs_in.FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    float distance = length(light.position - fs_in.FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    float shadow = ShadowCalculation(fs_in.LightSpacePos, norm, lightDir);

    vec3 lighting = ambient + (1.0 - shadow) * intensity * (diffuse + specular);
    lighting *= attenuation;

    FragColor = vec4(lighting, 1.0);
}
