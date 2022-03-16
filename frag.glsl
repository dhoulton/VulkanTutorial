#version 450

// Data in
layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 texcoord;

// Data out
layout(location = 0) out vec4 color;

// Uniforms
layout(binding = 1) uniform sampler2D tex;

void main()
{
	vec4 tcolor = texture(tex, texcoord);
	color = vec4(((frag_color * 0.25f) + (tcolor.rgb * 0.75)), 1.0);
}