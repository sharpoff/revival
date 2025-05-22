#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <stdio.h>

const bool enableDebugOutput = false;

class ContactListener : public JPH::ContactListener
{
    virtual JPH::ValidateResult	OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override
    {
        if (enableDebugOutput) {
            printf("[PHYSICS] Contact validate callback\n");
        }

        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

	virtual void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings)  override
    {
        if (enableDebugOutput) {
            printf("[PHYSICS] Contact added\n");
        }
    }

	virtual void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
    {
        if (enableDebugOutput) {
            printf("[PHYSICS] Contact persisted\n");
        }
    }

	virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
    {
        if (enableDebugOutput) {
            printf("[PHYSICS] Contact removed\n");
        }
    }
};

class BodyActivationListener : public JPH::BodyActivationListener
{
public:
	virtual void OnBodyActivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData) override
    {
        if (enableDebugOutput) {
            printf("[PHYSICS] Body activated\n");
        }
    }

	virtual void OnBodyDeactivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData) override
    {
        if (enableDebugOutput) {
            printf("[PHYSICS] Body deactivated\n");
        }
    }
};

