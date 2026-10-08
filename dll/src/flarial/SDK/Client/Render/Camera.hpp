// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/SDK/Client/Render/Camera.hpp: hooks that hold the game's own world matrix stack publish it in
// activeWorld, and modules that reach for the camera of the client instance get that one while the event runs, because
// ClientInstance::camera is not confirmed on 1.26.52.
#pragma once

#include "MatrixStack.hpp"
#include <Utils/Memory/Game/SignatureAndOffsetManager.hpp>

struct FrustumEdges
{
    glm::tvec3<float> topLeft;
    glm::tvec3<float> topRight;
    glm::tvec3<float> bottomLeft;
    glm::tvec3<float> bottomRight;
};

struct Frustum
{
    glm::tvec4<float> planes[6];
    FrustumEdges nearClippingPlaneEdges;
    FrustumEdges farClippingPlaneEdges;
};

namespace mce {
    class Camera {
    public:
        static inline thread_local MatrixStack *activeWorld = nullptr;

        MatrixStack viewMatrixStack;
        MatrixStack worldMatrixStack;
        MatrixStack projectionMatrixStack;

        glm::mat4 mInverseViewMatrix;
        glm::vec3 mRight;
        glm::vec3 mUp;
        glm::vec3 mForward;
        glm::vec3 mPosition;
        float mAspectRatio;
        float mFov;
        float mZNear;
        float mZFar;
        Frustum mFrustum;

        MatrixStack& getProjectionMatrixStack() { return projectionMatrixStack; }
        MatrixStack& getViewMatrixStack() { return viewMatrixStack; }
        MatrixStack& getWorldMatrixStack() { return activeWorld ? *activeWorld : worldMatrixStack; }
    };
}
