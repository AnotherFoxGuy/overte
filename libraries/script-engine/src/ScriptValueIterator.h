//
//  ScriptValueIterator.h
//  libraries/script-engine/src
//
//  Created by Heather Anderson on 5/2/21.
//  Copyright 2021 Vircadia contributors.
//  Copyright 2023 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//  SPDX-License-Identifier: Apache-2.0
//

/// @addtogroup ScriptEngine
/// @{

#ifndef hifi_ScriptValueIterator_h
#define hifi_ScriptValueIterator_h

#include <memory>

#include <QtCore/QString>

#include "ScriptValue.h"

class ScriptValueIterator;
using ScriptValueIteratorPointer = std::shared_ptr<ScriptValueIterator>;

/// [ScriptInterface] Provides an engine-independent interface for QScriptValueIterator
class ScriptValueIterator {
public:
    virtual ScriptValue::PropertyFlags flags() const = 0;
    virtual bool hasNext() const = 0;
    virtual QString name() const = 0;
    virtual void next() = 0;
    virtual ScriptValue value() const = 0;

protected:
    ~ScriptValueIterator() {}  // prevent explicit deletion of base class
};

#endif  // hifi_ScriptValueIterator_h

/// @}
