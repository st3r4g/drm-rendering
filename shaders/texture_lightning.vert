#version 300 es

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex;
uniform mat4 matrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 Tex;

void main()
{
	gl_Position = matrix * vec4(position, 1.0);
	FragPos = position;
	Normal = normal;
	Tex = tex;
}
