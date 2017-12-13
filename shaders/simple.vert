#version 320 es

layout(location = 0) in vec3 pos;
layout(location = 1) uniform mat3 rot;
out vec4 color_in;

void main()
{
	gl_Position = vec4(rot*pos, 1.0);
	color_in = vec4(0.5, 0.0, 0.0, 1.0);
}
