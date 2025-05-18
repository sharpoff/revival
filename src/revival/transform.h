#pragma once

#include <revival/types.h>

struct Transform
{
    Transform(vec3 position = vec3(0.0), quat orientation = quat(1.0, 0.0, 0.0, 0.0), vec3 scale = vec3(1.0));

    void rotate(float angle, vec3 axis);
    void rotate(quat q);

    void translate(vec3 pos);

    void scale(vec3 scale);

    mat4 getModelMatrix();

    vec3 &getPosition() { return m_position; };
    quat &getRotation() { return m_rotation; };
    vec3 &getScale() { return m_scale; };
private:
    vec3 m_position = vec3(0.0);
    quat m_rotation = quat(1.0, 0.0, 0.0, 0.0);
    vec3 m_scale = vec3(1.0);
};
