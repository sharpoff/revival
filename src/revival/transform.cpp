#include <revival/transform.h>

Transform::Transform(vec3 position, quat orientation, vec3 scale)
    : m_position(position), m_rotation(orientation), m_scale(scale)
{}

void Transform::rotate(float angle, vec3 axis)
{
    m_rotation *= glm::angleAxis(angle, axis);
}

void Transform::rotate(quat q)
{
    m_rotation *= q;
}

void Transform::translate(vec3 pos)
{
    m_position += pos;
}

void Transform::scale(vec3 scale)
{
    m_scale *= scale;
}

mat4 Transform::getModelMatrix()
{
    glm::mat4 translation = glm::translate(m_position);
    glm::mat4 rotation = glm::toMat4(m_rotation);
    glm::mat4 scale = glm::scale(m_scale);

    return translation * rotation * scale;
}
