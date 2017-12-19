#version 300 es

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
uniform mat4 camera;

out vec3 FragPos;
out vec3 Normal;

void main()
{
	gl_Position = camera * vec4(position, 1.0);
	FragPos = position;
	Normal = normal;
}
