#version 420 core

layout(location = 0) in vec2 a_Position;  // NDC 坐标，直接透传
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
	v_TexCoord  = a_TexCoord;
	gl_Position = vec4(a_Position, 0.0, 1.0);
}
