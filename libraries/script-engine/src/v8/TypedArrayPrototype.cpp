//
//  TypedArrayPrototype.cpp
//
//
//  Created by Clement on 7/14/14.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2023 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//  SPDX-License-Identifier: Apache-2.0
//

#include "TypedArrayPrototype.h"

#include <glm/common.hpp>

#include "TypedArrays.h"

// V8TODO Do not remove yet, this will be useful in later PRs
/*Q_DECLARE_METATYPE(QByteArray*)

TypedArrayPrototype::TypedArrayPrototype(QObject* parent) : QObject(parent) {
}

QByteArray* TypedArrayPrototype::thisArrayBuffer() const {
    V8ScriptValue bufferObject = thisObject().data().property(BUFFER_PROPERTY_NAME);
    return qscriptvalue_cast<QByteArray*>(bufferObject.data());
}

void TypedArrayPrototype::set(V8ScriptValue array, qint32 offset) {
    TypedArray* typedArray = static_cast<TypedArray*>(parent());
    if (array.isArray() || typedArray) {
        if (offset < 0) {
            engine()->evaluate("throw \"ArgumentError: negative offset\"");
        }
        quint32 length = array.property("length").toInt32();
        if (offset + (qint32)length > thisObject().data().property(typedArray->_lengthName).toInt32()) {
            engine()->evaluate("throw \"ArgumentError: array does not fit\"");
            return;
        }
        for (quint32 i = 0; i < length; ++i) {
            thisObject().setProperty(QString::number(offset + i), array.property(QString::number(i)));
        }
    } else {
        engine()->evaluate("throw \"ArgumentError: not an array\"");
    }
}

V8ScriptValue TypedArrayPrototype::subarray(qint32 begin) {
    TypedArray* typedArray = static_cast<TypedArray*>(parent());
    V8ScriptValue arrayBuffer = thisObject().data().property(typedArray->_bufferName);
    qint32 byteOffset = thisObject().data().property(typedArray->_byteOffsetName).toInt32();
    qint32 length = thisObject().data().property(typedArray->_lengthName).toInt32();
    qint32 bytesPerElement = typedArray->_bytesPerElement;
    
    // if indices < 0 then they start from the end of the array
    begin = (begin < 0) ? length + begin : begin;
    
    // here we clamp the indices to fit the array
    begin = glm::clamp(begin, 0, (length - 1));
    
    byteOffset += begin * bytesPerElement;
    return typedArray->newInstance(arrayBuffer, byteOffset, length - begin);
}

V8ScriptValue TypedArrayPrototype::subarray(qint32 begin, qint32 end) {
    TypedArray* typedArray = static_cast<TypedArray*>(parent());
    V8ScriptValue arrayBuffer = thisObject().data().property(typedArray->_bufferName);
    qint32 byteOffset = thisObject().data().property(typedArray->_byteOffsetName).toInt32();
    qint32 length = thisObject().data().property(typedArray->_lengthName).toInt32();
    qint32 bytesPerElement = typedArray->_bytesPerElement;
    
    // if indices < 0 then they start from the end of the array
    begin = (begin < 0) ? length + begin : begin;
    end = (end < 0) ? length + end : end;
    
    // here we clamp the indices to fit the array
    // note: begin offset is *inclusive* while end offset is *exclusive*
    // (see: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/subarray#Parameters)
    begin = glm::clamp(begin, 0, (length - 1));
    end = glm::clamp(end, 0, length);
    
    byteOffset += begin * bytesPerElement;
    length = (end - begin > 0) ? end - begin : 0;
    return typedArray->newInstance(arrayBuffer, byteOffset, length);
}

V8ScriptValue TypedArrayPrototype::get(quint32 index) {
    TypedArray* typedArray = static_cast<TypedArray*>(parent());
    V8ScriptString name = engine()->toStringHandle(QString::number(index));
    uint id;
    QScriptClass::QueryFlags flags = typedArray->queryProperty(thisObject(),
                                                               name,
                                                               QScriptClass::HandlesReadAccess, &id);
    if (QScriptClass::HandlesReadAccess & flags) {
        return typedArray->property(thisObject(), name, id);
    }
    return V8ScriptValue();
}

void TypedArrayPrototype::set(quint32 index, V8ScriptValue& value) {
    TypedArray* typedArray = static_cast<TypedArray*>(parent());
    V8ScriptValue object = thisObject();
    V8ScriptString name = engine()->toStringHandle(QString::number(index));
    uint id;
    QScriptClass::QueryFlags flags = typedArray->queryProperty(object,
                                                               name,
                                                               QScriptClass::HandlesWriteAccess, &id);
    if (QScriptClass::HandlesWriteAccess & flags) {
        typedArray->setProperty(object, name, id, value);
    }
}
*/
