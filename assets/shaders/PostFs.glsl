#version 420 core

layout(location = 0) out vec4 fragColor;

in vec2 v_TexCoord;

uniform sampler2D u_Screen;
uniform float     u_FlashIntensity;  // 0.0 ~ 1.0，死亡时红色闪烁

void main()
{
	vec4 color = texture(u_Screen, v_TexCoord);
	color.rgb = mix(color.rgb, vec3(1.0, 0.0, 0.0), u_FlashIntensity);
	fragColor = color;
}
