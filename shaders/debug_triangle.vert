#version 450

layout (push_constant) uniform Push
{
    vec3 vert[3];
};

void main()
{
    gl_Position = vec4(vert[gl_VertexIndex], 1.0);
}
