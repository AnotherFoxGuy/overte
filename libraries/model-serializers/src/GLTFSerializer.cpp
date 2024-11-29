//
//  GLTFSerializer.cpp
//  libraries/model-serializers/src
//
//  Created by Luis Cuenca on 8/30/17.
//  Copyright 2017 High Fidelity, Inc.
//  Copyright 2023 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#define CGLTF_IMPLEMENTATION

#include "GLTFSerializer.h"

#include <QtCore/QBuffer>
#include <QtCore/QIODevice>
#include <QtCore/QEventLoop>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qpair.h>
#include <QtCore/qlist.h>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

#include <qfile.h>
#include <qfileinfo.h>

#include <sstream>

#include <glm/gtx/transform.hpp>

#include <shared/NsightHelpers.h>
#include <NetworkAccessManager.h>
#include <ResourceManager.h>
#include <PathUtils.h>
#include <image/ColorChannel.h>
#include <BlendshapeConstants.h>
#include <procedural/ProceduralMaterialCache.h>

#include "FBXSerializer.h"

float atof_locale_independent(char* str) {
    //TODO: Once we have C++17 we can use std::from_chars
    std::istringstream streamToParse(str);
    streamToParse.imbue(std::locale("C"));
    float value;
    if (!(streamToParse >> value)) {
        qDebug(modelformat) << "cgltf: Cannot parse float from string: " << str;
        return 0.0f;
    }
    return value;
}

glm::mat4 GLTFSerializer::getModelTransform(const cgltf_node& node) {
    glm::mat4 tmat = glm::mat4(1.0);

    if (node.has_matrix) {
        tmat = glm::mat4(node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3],
            node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7],
            node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
            node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15]);
    } else {

        if (node.has_scale) {
            glm::vec3 scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            glm::mat4 s = glm::mat4(1.0);
            s = glm::scale(s, scale);
            tmat = s * tmat;
        }

        if (node.has_rotation) {
            //quat(x,y,z,w) to quat(w,x,y,z)
            glm::quat rotquat = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            tmat = glm::mat4_cast(rotquat) * tmat;
        }

        if (node.has_translation) {
            glm::vec3 trans = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            glm::mat4 t = glm::mat4(1.0);
            t = glm::translate(t, trans);
            tmat = t * tmat;
        }
    }
    return tmat;
}

bool GLTFSerializer::getSkinInverseBindMatrices(std::vector<std::vector<float>>& inverseBindMatrixValues) {
    for (size_t i = 0; i < _data->skins_count; i++) {
        auto &skin = _data->skins[i];

        if (skin.inverse_bind_matrices == NULL) {
            return false;
        }

        cgltf_accessor &matricesAccessor = *skin.inverse_bind_matrices;
        QVector<float> matrices;
        if (matricesAccessor.type != cgltf_type_mat4) {
            return false;
        }
        matrices.resize((int)matricesAccessor.count * 16);
        size_t numFloats = cgltf_accessor_unpack_floats(&matricesAccessor, matrices.data(), matricesAccessor.count * 16);
        Q_ASSERT(numFloats == matricesAccessor.count * 16);
        inverseBindMatrixValues.push_back(std::vector<float>(matrices.begin(), matrices.end()));
    }
    return true;
}

bool GLTFSerializer::generateTargetData(cgltf_accessor *accessor, float weight, QVector<glm::vec3>& returnVector) {
    QVector<float> storedValues;
    if(accessor == nullptr) {
        return false;
    }
    if (accessor->type != cgltf_type_vec3) {
        return false;
    }
    storedValues.resize((int)accessor->count * 3);
    size_t numFloats = cgltf_accessor_unpack_floats(accessor, storedValues.data(), accessor->count * 3);
    if (numFloats != accessor->count * 3) {
        return false;
    }

    for (int n = 0; n + 2 < storedValues.size(); n = n + 3) {
        returnVector.push_back(glm::vec3(weight * storedValues[n], weight * storedValues[n + 1], weight * storedValues[n + 2]));
    }
    return true;
}

bool GLTFSerializer::findNodeInPointerArray(const cgltf_node *nodePointer, cgltf_node **nodes, size_t arraySize, size_t &index) {
    for (size_t i = 0; i < arraySize; i++) {
        if (nodes[i] == nodePointer) {
            index = i;
            return true;
        }
    }
    return false;
}


bool GLTFSerializer::findAttribute(const QString &name, const cgltf_attribute *attributes, size_t numAttributes, size_t &index) {
    std::string nameString = name.toStdString();
    for (size_t i = 0; i < numAttributes; i++) {
        if (attributes->name == nullptr) {
            qDebug(modelformat) << "GLTFSerializer: attribute with a null pointer name string";
        } else {
            if (strcmp(nameString.c_str(), attributes->name) == 0) {
                index = i;
                return true;
            }
        }
    }
    return false;
}

MediaType GLTFSerializer::getMediaType() const {
    MediaType mediaType("gltf");
    mediaType.extensions.push_back("gltf");
    mediaType.webMediaTypes.push_back("model/gltf+json");

    mediaType.extensions.push_back("glb");
    mediaType.webMediaTypes.push_back("model/gltf-binary");

    mediaType.extensions.push_back("vrm");
    mediaType.webMediaTypes.push_back("model/gltf-binary");

    return mediaType;
}

std::unique_ptr<hfm::Serializer::Factory> GLTFSerializer::getFactory() const {
    return std::make_unique<hfm::Serializer::SimpleFactory<GLTFSerializer>>();
}

HFMModel::Pointer GLTFSerializer::read(const hifi::ByteArray& data, const hifi::VariantHash& mapping, const hifi::URL& url) {

    _url = url;

    // Normalize url for local files
    hifi::URL normalizeUrl = DependencyManager::get<ResourceManager>()->normalizeURL(_url);
    if (normalizeUrl.scheme().isEmpty() || (normalizeUrl.scheme() == "file")) {
        QString localFileName = PathUtils::expandToLocalDataAbsolutePath(normalizeUrl).toLocalFile();
        _url = hifi::URL(QFileInfo(localFileName).absoluteFilePath());
    }

    cgltf_options options = {};
    cgltf_result result = cgltf_parse(&options, data.data(), data.size(), &_data);
    if (result != cgltf_result_success) {
        qCDebug(modelformat) << "Error parsing GLTF file.";
        return nullptr;
    }
    cgltf_load_buffers(&options, _data, NULL);
    for (size_t i = 0; i < _data->buffers_count; i++) {
        cgltf_buffer &buffer = _data->buffers[i];
        if (buffer.data == nullptr) {
            if (!readBinary(buffer.uri, buffer)) {
                qCDebug(modelformat) << "Error parsing GLTF file.";
                return nullptr;
            }
        }
    }


    auto hfmModelPtr = std::make_shared<HFMModel>();
    HFMModel& hfmModel = *hfmModelPtr;
    buildGeometry(hfmModel, mapping, _url);

    return hfmModelPtr;
}

bool GLTFSerializer::readBinary(const QString& url, cgltf_buffer &buffer) {
    bool success;
    hifi::ByteArray outdata;

    // Is this part already done by cgltf?
    if (url.contains("data:application/octet-stream;base64,")) {
        qDebug() << "GLTFSerializer::readBinary: base64";
        outdata = requestEmbeddedData(url);
        success = !outdata.isEmpty();
    } else {
        hifi::URL binaryUrl = _url.resolved(url);
        std::tie<bool, hifi::ByteArray>(success, outdata) = requestData(binaryUrl);
    }
    if (success) {
        if(buffer.size == (size_t)outdata.size()) {
            _externalData.push_back(outdata);
            buffer.data = _externalData.last().data();
            buffer.data_free_method = cgltf_data_free_method_none;
        } else {
            qDebug() << "Buffer size mismatch for model: " << _url;
            success = false;
        }
    }

    return success;
}

std::tuple<bool, hifi::ByteArray> GLTFSerializer::requestData(hifi::URL& url) {
    auto request = DependencyManager::get<ResourceManager>()->createResourceRequest(
        nullptr, url, true, -1, "GLTFSerializer::requestData");

    if (!request) {
        return std::make_tuple(false, hifi::ByteArray());
    }

    QEventLoop loop;
    QObject::connect(request, &ResourceRequest::finished, &loop, &QEventLoop::quit);
    request->send();
    loop.exec();

    if (request->getResult() == ResourceRequest::Success) {
        return std::make_tuple(true, request->getData());
    } else {
        return std::make_tuple(false, hifi::ByteArray());
    }
}

hifi::ByteArray GLTFSerializer::requestEmbeddedData(const QString& url) {
    QString binaryUrl = url.split(",")[1];
    return binaryUrl.isEmpty() ? hifi::ByteArray() : QByteArray::fromBase64(binaryUrl.toUtf8());
}


QNetworkReply* GLTFSerializer::request(hifi::URL& url, bool isTest) {
    if (!qApp) {
        return nullptr;
    }
    bool aboutToQuit{ false };
    auto connection = QObject::connect(qApp, &QCoreApplication::aboutToQuit, [&] {
        aboutToQuit = true;
    });
    QNetworkAccessManager& networkAccessManager = NetworkAccessManager::getInstance();
    QNetworkRequest netRequest(url);
    netRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* netReply = isTest ? networkAccessManager.head(netRequest) : networkAccessManager.get(netRequest);
    if (!qApp || aboutToQuit) {
        netReply->deleteLater();
        return nullptr;
    }
    QEventLoop loop; // Create an event loop that will quit when we get the finished signal
    QObject::connect(netReply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();                    // Nothing is going to happen on this whole run thread until we get this

    QObject::disconnect(connection);
    return netReply;                // trying to sync later on.
}

void GLTFSerializer::retriangulate(const QVector<int>& inIndices, const QVector<glm::vec3>& in_vertices,
                               const QVector<glm::vec3>& in_normals, QVector<int>& outIndices,
                               QVector<glm::vec3>& out_vertices, QVector<glm::vec3>& out_normals) {
    for (int i = 0; i + 2 < inIndices.size(); i = i + 3) {

        int idx1 = inIndices[i];
        int idx2 = inIndices[i+1];
        int idx3 = inIndices[i+2];

        out_vertices.push_back(in_vertices[idx1]);
        out_vertices.push_back(in_vertices[idx2]);
        out_vertices.push_back(in_vertices[idx3]);

        out_normals.push_back(in_normals[idx1]);
        out_normals.push_back(in_normals[idx2]);
        out_normals.push_back(in_normals[idx3]);

        outIndices.push_back(i);
        outIndices.push_back(i+1);
        outIndices.push_back(i+2);
    }
}

GLTFSerializer::~GLTFSerializer() {
    cgltf_free(_data);
}