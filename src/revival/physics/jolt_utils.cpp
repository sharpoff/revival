#include <revival/physics/jolt_utils.h>

namespace JoltUtils
{

glm::vec3 toGLMVec3(JPH::Vec3 vec)
{
    return vec3(vec.GetX(), vec.GetY(), vec.GetZ());
}

} // namespace JoltUtils
