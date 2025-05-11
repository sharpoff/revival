#include <revival/physics/physics.h>
#include <revival/physics/jolt_utils.h>

PhysicsSystem::PhysicsSystem()
{
    // Register allocation hook.
    JPH::RegisterDefaultAllocator();

    JPH::Factory::sInstance = new JPH::Factory();

    // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
    JPH::RegisterTypes();

    // We need a temp allocator for temporary allocations during the physics update. We're
    // pre-allocating 10 MB to avoid having to do allocations during the physics update.
    tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

    // We need a job system that will execute physics jobs on multiple threads.
    jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, JPH::thread::hardware_concurrency() - 1);

    physicsSystem.Init(maxBodies, numBodyMutexes, maxBodies, maxContactConstraints, broadPhaseLayerInterface, objectVsBroadPhaseLayerFilter, objectVsObjectFilter);

    physicsSystem.SetBodyActivationListener(&bodyActivationListener);
    physicsSystem.SetContactListener(&contactListener);

    JPH::BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();

    // Create static floor shape
    // JPH::BoxShapeSettings floorShapeSettings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    // JPH::ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
    // JPH::ShapeRefC floorShape = floorShapeResult.Get();

    // Create rigid body of a static floor
    JPH::BodyCreationSettings floorSettings(new JPH::BoxShape(JPH::Vec3(100.0f, 1.0f, 100.0f)), JPH::Vec3(0.0, -1.0, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NON_MOVING);
    floorID = bodyInterface.CreateAndAddBody(floorSettings, JPH::EActivation::DontActivate);

    // Create rigid body of a dynamic sphere
    JPH::BodyCreationSettings sphereSettings(new JPH::SphereShape(0.5f), JPH::Vec3(0.0, 50.0, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING);
    sphereID = bodyInterface.CreateAndAddBody(sphereSettings, JPH::EActivation::Activate);

    bodyInterface.SetLinearVelocity(sphereID, JPH::Vec3(0.0f, -5.0f, 0.0f));
}

PhysicsSystem::~PhysicsSystem()
{
    JPH::BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();

    bodyInterface.RemoveBody(floorID);
    bodyInterface.DestroyBody(floorID);

    bodyInterface.RemoveBody(sphereID);
    bodyInterface.DestroyBody(sphereID);

    JPH::UnregisterTypes();

    delete tempAllocator;
    delete jobSystem;
}

void PhysicsSystem::update(float dt)
{
    JPH::BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();

    float physicsDeltaTime = 1.0f / 60.0f;

    if (bodyInterface.IsActive(sphereID)) {
        JPH::Vec3 position = bodyInterface.GetPosition(sphereID);

        printf("[PHYSICS] sphere pos: [%f, %f, %f]\n", position.GetX(), position.GetY(), position.GetZ());

        physicsSystem.Update(physicsDeltaTime, 1, tempAllocator, jobSystem);
    }
}

glm::vec3 PhysicsSystem::getFloorPos()
{
    JPH::BodyInterface &bodyInterface = physicsSystem.GetBodyInterface();
    return JoltUtils::toGLMVec3(bodyInterface.GetPosition(sphereID));
}
