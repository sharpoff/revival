#pragma once

#include <revival/math/math.h>
#include <GLFW/glfw3.h>

struct Camera
{
    void update(GLFWwindow *window, float deltaTime);

    void setProjection(mat4 projection);
    void setPosition(vec3 position);
    void setMovementSpeed(float speed);
    void setRotationSpeed(float speed);

    mat4 getView() { return view; };
    mat4 getProjection() { return projection; };
    vec3 getPosition() { return position; };
    vec3 getRight() { return vec3(view[0][0], view[1][0], view[2][0]); };
    vec3 getUp() { return vec3(view[0][1], view[1][1], view[2][1]); };
    vec3 getFront() { return front; };
private:
    vec3 position = vec3(0.0f);
    vec3 front = vec3(0, 0, -1);
    vec3 up = vec3(0, 1, 0);

    float pitch = 0.0f;
    float yaw = 0.0f;

    float movementSpeed = 10.0;
    float rotationSpeed = glm::radians(10.0f);
    vec2 cursorPrevious = vec2(0.0);

    mat4 projection = mat4(1.0);
    mat4 view = mat4(1.0);

    struct
    {
        bool right = false;
        bool left = false;
        bool up = false;
        bool down = false;
        bool shift = false;
    } keys;
};
