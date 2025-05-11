#pragma once

#include <revival/math/math.h>
#include <GLFW/glfw3.h>

class Camera
{
public:
    void update(float deltaTime);
    void handleInput(GLFWwindow *window, float deltaTime);
    mat4 getView();
    mat4 getRotationMatrix();

    void setPerspective(float fov, float aspectRatio, float near, float far);
    void setPosition(vec3 position);
    void setMovementSpeed(float speed);
    void setRotationSpeed(float speed);

    mat4 getProjection();
    vec3 getPosition();
private:
    vec3 position = vec3(0.0f);
    glm::vec3 velocity = vec3(0.0f);
    float pitch = 0.0f;
    float yaw = 0.0f;

    float movementSpeed = 3.0;
    float rotationSpeed = 0.01;
    vec2 cursorPrevious = vec2(0.0);

    mat4 projection = mat4(1.0);

    struct
    {
        bool right = false;
        bool left = false;
        bool up = false;
        bool down = false;
        bool shift = false;
    } keys;
};
