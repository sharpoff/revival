#version 450

layout (push_constant) uniform Push
{
    vec3 from;
    vec3 to;
};

void main()
{
    if (gl_VertexIndex == 0) {
        gl_Position = vec4(from, 1.0);
    } else if (gl_VertexIndex == 1) {
        gl_Position = vec4(to, 1.0);
    }
}
