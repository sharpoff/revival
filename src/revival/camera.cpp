#include <revival/camera.h>
#include "imgui.h"

void Camera::update(GLFWwindow *window, double deltaTime)
{
    // keyboard
    keys.shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) != GLFW_RELEASE;
    keys.left = glfwGetKey(window, GLFW_KEY_A) != GLFW_RELEASE;
    keys.right = glfwGetKey(window, GLFW_KEY_D) != GLFW_RELEASE;
    keys.up = glfwGetKey(window, GLFW_KEY_W) != GLFW_RELEASE;
    keys.down = glfwGetKey(window, GLFW_KEY_S) != GLFW_RELEASE;

    // mouse
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
        vec2 motion = vec2(xpos - cursorPrevious.x, ypos - cursorPrevious.y);

        yaw += motion.x * rotationSpeed;
        pitch -= motion.y * rotationSpeed;

        pitch = glm::clamp(pitch, -89.9f, 89.9f);
    }

    cursorPrevious.x = xpos;
    cursorPrevious.y = ypos;

    float speedBoost = 1.0f;
    if (keys.shift)
        speedBoost = 4.0f;

    // spherical coords to cartesian
    front = {
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
    };
    front = glm::normalize(front);

    vec3 right = glm::cross(front, up);

    float speed = movementSpeed * deltaTime * speedBoost;
    if (keys.up) {
        position += front * speed;
    }
    if (keys.down) {
        position -= front * speed;
    }
    if (keys.left) {
        position -= right * speed;
    }
    if (keys.right) {
        position += right * speed;
    }

    view = glm::lookAt(position, position + front, up);
}

void Camera::setProjection(mat4 projection)
{
    this->projection = projection;
}

void Camera::setPosition(vec3 position)
{
    this->position = position;
}

void Camera::setMovementSpeed(float speed)
{
    this->movementSpeed = speed;
}

void Camera::setRotationSpeed(float speed)
{
    this->rotationSpeed = speed;
}
