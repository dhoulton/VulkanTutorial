#version 450

// Data in
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_texcoord;

// Data out
layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 out_texcoord;

// Uniforms
layout(binding = 0) uniform UniformBufferObject
{
    vec2 foo;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_position, 0.0, 1.0);
    out_texcoord = in_texcoord;
    frag_color = in_color;
}