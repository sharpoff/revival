#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <Jolt/Renderer/DebugRendererSimple.h>

#include <revival/physics/physics_layers.h>
#include <revival/physics/physics_listeners.h>
#include <revival/physics/rigid_body.h>
#include <revival/physics/helpers.h>
#include <revival/math/math.h>
#include <revival/transform.h>
#include <revival/game_object.h>

class Physics
{
public:
    bool init();
    void shutdown(std::vector<GameObject> &gameObjects);

    // void drawBodies(JPH::DebugRendererSimple *debugRenderer);

    void update(float dt, std::vector<GameObject> &gameObjects);

    void createBox(RigidBody *body, Transform transform, vec3 halfExtent, bool isStatic);
    Transform getTransform(JPH::BodyID id);
private:
    JPH::JobSystemThreadPool *jobSystem;
    JPH::TempAllocatorImpl *tempAllocator;

    // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
    // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
    const uint maxBodies = 1024;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
    const uint numBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
    // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase).
    const uint maxContactConstraints = 1024;

    // Create mapping table from object layer to broadphase layer
    BPLayerInterfaceImpl broadPhaseLayerInterface;

    // Create class that filters object vs broadphase layers
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;

    // Create class that filters object vs object layers
    ObjectLayerPairFilterImpl objectVsObjectFilter;

    JPH::PhysicsSystem physicsSystem;

    BodyActivationListener bodyActivationListener;

    ContactListener contactListener;
};
