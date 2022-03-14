#version 450

// Data in
layout(location = 0) in vec3 frag_color;

// Data out
layout(location = 0) out vec4 color;

void main()
{
	color = vec4(frag_color, 1.0);
}