//
//  Created by Bradley Austin Davis on 2015/10/18
//  (based on UserInputMapper inner class created by Sam Gateau on 4/27/15)
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2023 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//  SPDX-License-Identifier: Apache-2.0
//

#pragma once
#ifndef hifi_controllers_Pose_h
#define hifi_controllers_Pose_h
#include <ScriptValue.h>

class ScriptEngine;

#include <GLMHelpers.h>

namespace controller {

    struct Pose {
    public:
        vec3 translation;
        quat rotation;
        vec3 velocity;
        vec3 angularVelocity;
        bool valid{ false };

        Pose() {}
        Pose(const vec3& translation, const quat& rotation,
             const vec3& velocity = vec3(), const vec3& angularVelocity = vec3());

        Pose(const Pose&) = default;
        Pose& operator = (const Pose&) = default;
        bool operator ==(const Pose& right) const;
        bool operator !=(const Pose& right) const { return !(*this == right); }
        bool isValid() const { return valid; }
        vec3 getTranslation() const { return translation; }
        quat getRotation() const { return rotation; }
        vec3 getVelocity() const { return velocity; }
        vec3 getAngularVelocity() const { return angularVelocity; }
        mat4 getMatrix() const { return createMatFromQuatAndPos(rotation, translation); }

        Pose transform(const glm::mat4& mat) const;
        Pose postTransform(const glm::mat4& mat) const;

        static ScriptValue toScriptValue(ScriptEngine* engine, const Pose& event);
        static bool fromScriptValue(const ScriptValue& object, Pose& event);
    };
}

//Q_DECLARE_METATYPE(controller::Pose);

#endif
