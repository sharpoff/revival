#include <revival/camera.h>
#include "imgui.h"

void Camera::update(float deltaTime)
{
    pitch = glm::clamp(pitch, glm::radians(-89.9f), glm::radians(89.9f));

    mat4 rotation = getRotationMatrix();

    float speedBoost = 1.0f;
    if (keys.shift)
        speedBoost = 4.0f;

    position += vec3(rotation * vec4(velocity * movementSpeed * speedBoost * deltaTime, 0.0f));
}

void Camera::handleInput(GLFWwindow *window, float deltaTime)
{
    // keyboard
    keys.shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) != GLFW_RELEASE;
    keys.left = glfwGetKey(window, GLFW_KEY_A) != GLFW_RELEASE;
    keys.right = glfwGetKey(window, GLFW_KEY_D) != GLFW_RELEASE;
    keys.up = glfwGetKey(window, GLFW_KEY_W) != GLFW_RELEASE;
    keys.down = glfwGetKey(window, GLFW_KEY_S) != GLFW_RELEASE;

    vec2 cameraMotion = vec2(keys.up, keys.right) - vec2(keys.down, keys.left);

    velocity = vec3(cameraMotion.y, 0.0, -cameraMotion.x);

    // mouse
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
        vec2 motion = vec2(xpos - cursorPrevious.x, ypos - cursorPrevious.y);

        yaw += motion.x * rotationSpeed;
        pitch -= motion.y * rotationSpeed;
    }

    cursorPrevious.x = xpos;
    cursorPrevious.y = ypos;
}

mat4 Camera::getView()
{
    mat4 translation = translate(mat4(1.0f), position);
    mat4 rotation = getRotationMatrix();
    return glm::inverse(translation * rotation);
}

mat4 Camera::getRotationMatrix()
{
    glm::quat pitchRotation = glm::angleAxis(pitch, vec3(1.0f, 0.0f, 0.0f));
    glm::quat yawRotation = glm::angleAxis(yaw, vec3(0.0f, -1.0f, 0.0f));

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::setPerspective(float fov, float aspectRatio, float near, float far)
{
    projection = math::perspective(glm::radians(fov), aspectRatio, near, far);
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

mat4 Camera::getProjection()
{
    return projection;
}

vec3 Camera::getPosition()
{
    return position;
}
