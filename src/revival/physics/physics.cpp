#include <revival/physics/physics.h>

using namespace JPH;

bool Physics::init()
{
    // Register allocation hook.
    RegisterDefaultAllocator();

    Factory::sInstance = new Factory();

    // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
    RegisterTypes();

    // We need a temp allocator for temporary allocations during the physics update. We're
    // pre-allocating 10 MB to avoid having to do allocations during the physics update.
    tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024);

    // We need a job system that will execute physics jobs on multiple threads.
    jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

    physicsSystem.Init(maxBodies, numBodyMutexes, maxBodies, maxContactConstraints, broadPhaseLayerInterface, objectVsBroadPhaseLayerFilter, objectVsObjectFilter);

    physicsSystem.SetBodyActivationListener(&bodyActivationListener);
    physicsSystem.SetContactListener(&contactListener);

    printf("Successfully initialized physics.\n");

    return true;
}

void Physics::shutdown(std::vector<GameObject> &gameObjects)
{
    BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();

    for (auto &gameObject : gameObjects) {
        bodyInterface.RemoveBody(gameObject.rigidBody.bodyId);
        bodyInterface.DestroyBody(gameObject.rigidBody.bodyId);
    }

    UnregisterTypes();

    delete tempAllocator;
    delete jobSystem;
}

// void Physics::drawBodies(DebugRendererSimple *debugRenderer)
// {
//     physicsSystem.DrawBodies(BodyManager::DrawSettings(), debugRenderer);
// }

void Physics::createBox(RigidBody *body, Transform transform, vec3 halfExtent, bool isStatic)
{
    if (!body) return;

    BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();

    BoxShapeSettings settings(toJolt(halfExtent));
    settings.SetEmbedded();

    Shape::ShapeResult result = settings.Create();
    if (result.IsValid()) {
        body->shape = result.Get();
    } else {
        printf("[PHYSICS] Failed to create body shape.\n");
        return;
    }

    vec3 position = transform.getPosition();
    quat rotation = transform.getRotation();

    if (isStatic) {
        body->bodyId = bodyInterface.CreateAndAddBody(BodyCreationSettings(body->shape.GetPtr(), toJolt(position), toJolt(rotation), EMotionType::Static, Layers::NON_MOVING), EActivation::DontActivate);
    } else {
        body->bodyId = bodyInterface.CreateAndAddBody(BodyCreationSettings(body->shape.GetPtr(), toJolt(position), toJolt(rotation), EMotionType::Dynamic, Layers::MOVING), EActivation::Activate);

        bodyInterface.SetLinearVelocity(body->bodyId, Vec3(0.0f, -5.0f, 0.0f));
    }
}

void Physics::update(float dt, std::vector<GameObject> &gameObjects)
{
    BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();

    float physicsDeltaTime = 1.0f / 60.0f;

    bool isActive = false;
    for (auto &object : gameObjects) {
        RigidBody &rigidBody = object.rigidBody;

        if (!rigidBody.isStatic && bodyInterface.IsActive(rigidBody.bodyId)) {
            object.transform = getTransform(rigidBody.bodyId);

            printf("%s - [%f, %f, %f]\n", object.name.c_str(), object.transform.getPosition().x, object.transform.getPosition().y, object.transform.getPosition().z);

            isActive = true;
        }
    }

    if (isActive)
        physicsSystem.Update(physicsDeltaTime, 1, tempAllocator, jobSystem);
}

Transform Physics::getTransform(JPH::BodyID id)
{
    BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();
    vec3 position = toGlm(bodyInterface.GetPosition(id));
    quat rotation = toGlm(bodyInterface.GetRotation(id));

    return Transform(position, rotation, vec3(1.0));
}
