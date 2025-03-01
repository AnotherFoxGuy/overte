//
//  GLTFSerializer_VRM.cpp
//  libraries/model-serializers/src
//
//  Created by Edgar on 29/11/24.
//  Copyright 2024 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GLTFSerializer.h"

#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qpair.h>
#include <QtCore/qlist.h>


bool GLTFSerializer::loadVRMData(HFMModel& hfmModel, const cgltf_extension& extension) {
    qCDebug(modelformat) << "Extension data found: " << extension.data;

    QJsonDocument data = QJsonDocument::fromJson(extension.data);

    if (data.isNull())
        return false;

    if (data["authors"].isArray()) {
        auto authors = data["authors"].toArray();
        for (auto&& a : authors) {
            hfmModel.author.append(a.toString());
        }
    }

    return true;
}