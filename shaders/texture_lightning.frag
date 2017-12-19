#version 300 es

precision mediump float;

in vec3 FragPos;
in vec3 Normal;
in vec2 Tex;
out vec4 fragColor;

uniform sampler2D ourTexture;
uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;

void main()
{
	float ambientStrength = 0.1;
	vec3 ambient = ambientStrength * lightColor;

	vec3 norm = normalize(Normal);
	vec3 lightDir = normalize(lightPos - FragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * lightColor;

	vec3 result = (ambient + diffuse) * objectColor;
//	fragColor = vec4(result, 1.0);
	fragColor = texture(ourTexture, Tex);
}
