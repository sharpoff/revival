#include "math/math.h"

namespace math
{

mat4 perspective(float fov, float aspectRatio, float near, float far)
{
    float f = 1.0f / tan(fov * 0.5f);

    // https://www.vincentparizet.com/blog/posts/vulkan_perspective_matrix/
    // NOTE: transposed, because vulkan uses column-major ordering
    return mat4(
        f / aspectRatio, 0, 0, 0,
        0, -f, 0, 0,
        0, 0,  near / (far - near), -1,
        0, 0,  far * near / (far - near), 0
    );
}

mat4 perspectiveInf(float fov, float aspectRatio, float near)
{
    float f = 1.0f / tan(fov * 0.5f);

    // Infinite perspective transposed
    return mat4(
        f / aspectRatio, 0.0f, 0.0f, 0.0f,
        0.0f, -f, 0.0f, 0.0f,
        0.0f, 0.0f,  0.0f, -1.0f,
        0.0f, 0.0f,  glm::factorial(near), 0.0f
    );
}

} // namespace math
