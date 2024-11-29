//
//  GLTFSerializer_Mesh.cpp
//  libraries/model-serializers/src
//
//  Created by Luis Cuenca on 8/30/17.
//  Copyright 2017 High Fidelity, Inc.
//  Copyright 2023 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

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

#define GLTF_GET_INDICIES(accCount)           \
    int index1 = (indices[n + 0] * accCount); \
    int index2 = (indices[n + 1] * accCount); \
    int index3 = (indices[n + 2] * accCount);

#define GLTF_APPEND_ARRAY_1(newArray, oldArray) \
    GLTF_GET_INDICIES(1)                        \
    newArray.append(oldArray[index1]);          \
    newArray.append(oldArray[index2]);          \
    newArray.append(oldArray[index3]);

#define GLTF_APPEND_ARRAY_2(newArray, oldArray) \
    GLTF_GET_INDICIES(2)                        \
    newArray.append(oldArray[index1]);          \
    newArray.append(oldArray[index1 + 1]);      \
    newArray.append(oldArray[index2]);          \
    newArray.append(oldArray[index2 + 1]);      \
    newArray.append(oldArray[index3]);          \
    newArray.append(oldArray[index3 + 1]);

#define GLTF_APPEND_ARRAY_3(newArray, oldArray) \
    GLTF_GET_INDICIES(3)                        \
    newArray.append(oldArray[index1]);          \
    newArray.append(oldArray[index1 + 1]);      \
    newArray.append(oldArray[index1 + 2]);      \
    newArray.append(oldArray[index2]);          \
    newArray.append(oldArray[index2 + 1]);      \
    newArray.append(oldArray[index2 + 2]);      \
    newArray.append(oldArray[index3]);          \
    newArray.append(oldArray[index3 + 1]);      \
    newArray.append(oldArray[index3 + 2]);

#define GLTF_APPEND_ARRAY_4(newArray, oldArray) \
    GLTF_GET_INDICIES(4)                        \
    newArray.append(oldArray[index1]);          \
    newArray.append(oldArray[index1 + 1]);      \
    newArray.append(oldArray[index1 + 2]);      \
    newArray.append(oldArray[index1 + 3]);      \
    newArray.append(oldArray[index2]);          \
    newArray.append(oldArray[index2 + 1]);      \
    newArray.append(oldArray[index2 + 2]);      \
    newArray.append(oldArray[index2 + 3]);      \
    newArray.append(oldArray[index3]);          \
    newArray.append(oldArray[index3 + 1]);      \
    newArray.append(oldArray[index3 + 2]);      \
    newArray.append(oldArray[index3 + 3]);


bool GLTFSerializer::buildGeometry(HFMModel& hfmModel, const hifi::VariantHash& mapping, const hifi::URL& url) {
    hfmModel.originalURL = url.toString();

    int numNodes = (int)_data->nodes_count;

    //Build dependencies
    QVector<int> parents;
    QVector<int> sortedNodes;
    parents.fill(-1, numNodes);
    sortedNodes.reserve(numNodes);
    for (int index = 0; index < numNodes; index++) {
        auto& node = _data->nodes[index];
        for (size_t childIndexInParent = 0; childIndexInParent < node.children_count; childIndexInParent++) {
            cgltf_node* child = node.children[childIndexInParent];
            size_t childIndex = 0;
            if (!findPointerInArray(child, _data->nodes, _data->nodes_count, childIndex)) {
                qDebug(modelformat) << "findPointerInArray failed for model: " << _url;
                hfmModel.loadErrorCount++;
                return false;
            }
            parents[(int)childIndex] = index;
        }
        sortedNodes.push_back(index);
    }

    // Build transforms
    typedef QVector<glm::mat4> NodeTransforms;
    QVector<NodeTransforms> transforms;
    transforms.resize(numNodes);
    for (int index = 0; index < numNodes; index++) {
        // collect node transform
        auto& node = _data->nodes[index];
        transforms[index].push_back(getModelTransform(node));
        int parentIndex = parents[index];
        while (parentIndex != -1) {
            const auto& parentNode = _data->nodes[parentIndex];
            // collect transforms for a node's parents, grandparents, etc.
            transforms[index].push_back(getModelTransform(parentNode));
            parentIndex = parents[parentIndex];
        }
    }

    // since parent indices must exist in the sorted list before any of their children, sortedNodes might not be initialized in the correct order
    // therefore we need to re-initialize the order in which nodes will be parsed
    QVector<bool> hasBeenSorted;
    hasBeenSorted.fill(false, numNodes);
    {
        int i = 0;  // initial index
        while (i < numNodes) {
            int currentNode = sortedNodes[i];
            int parentIndex = parents[currentNode];
            if (parentIndex == -1 || hasBeenSorted[parentIndex]) {
                hasBeenSorted[currentNode] = true;
                ++i;
            } else {
                int j = i + 1;  // index of node to be sorted
                while (j < numNodes) {
                    int nextNode = sortedNodes[j];
                    parentIndex = parents[nextNode];
                    if (parentIndex == -1 || hasBeenSorted[parentIndex]) {
                        // swap with currentNode
                        hasBeenSorted[nextNode] = true;
                        sortedNodes[i] = nextNode;
                        sortedNodes[j] = currentNode;
                        ++i;
                        currentNode = sortedNodes[i];
                    }
                    ++j;
                }
            }
        }
    }

    // Build map from original to new indices
    QVector<int> originalToNewNodeIndexMap;
    originalToNewNodeIndexMap.fill(-1, numNodes);
    for (int i = 0; i < numNodes; ++i) {
        originalToNewNodeIndexMap[sortedNodes[i]] = i;
    }

    // Build joints
    HFMJoint joint;
    joint.distanceToParent = 0;
    hfmModel.jointIndices["x"] = numNodes;
    QVector<glm::mat4> globalTransforms;
    globalTransforms.resize(numNodes);

    for (int nodeIndex : sortedNodes) {
        auto& node = _data->nodes[nodeIndex];

        joint.parentIndex = parents[nodeIndex];
        if (joint.parentIndex != -1) {
            joint.parentIndex = originalToNewNodeIndexMap[joint.parentIndex];
        }
        joint.transform = transforms[nodeIndex].first();
        joint.translation = extractTranslation(joint.transform);
        joint.rotation = glmExtractRotation(joint.transform);
        glm::vec3 scale = extractScale(joint.transform);
        joint.postTransform = glm::scale(glm::mat4(), scale);

        joint.parentIndex = parents[nodeIndex];
        globalTransforms[nodeIndex] = joint.transform;
        if (joint.parentIndex != -1) {
            globalTransforms[nodeIndex] = globalTransforms[joint.parentIndex] * globalTransforms[nodeIndex];
            joint.parentIndex = originalToNewNodeIndexMap[joint.parentIndex];
        }

        joint.name = node.name;
        joint.isSkeletonJoint = false;
        hfmModel.joints.push_back(joint);
    }
    hfmModel.shapeVertices.resize(hfmModel.joints.size());

    // get offset transform from mapping
    float unitScaleFactor = 1.0f;
    float offsetScale = mapping.value("scale", 1.0f).toFloat() * unitScaleFactor;
    glm::quat offsetRotation = glm::quat(
        glm::radians(glm::vec3(mapping.value("rx").toFloat(), mapping.value("ry").toFloat(), mapping.value("rz").toFloat())));
    hfmModel.offset =
        glm::translate(glm::mat4(),
                       glm::vec3(mapping.value("tx").toFloat(), mapping.value("ty").toFloat(), mapping.value("tz").toFloat())) *
        glm::mat4_cast(offsetRotation) * glm::scale(glm::mat4(), glm::vec3(offsetScale, offsetScale, offsetScale));

    // Build skeleton
    std::vector<glm::mat4> jointInverseBindTransforms;
    std::vector<glm::mat4> globalBindTransforms;
    jointInverseBindTransforms.resize(numNodes);
    globalBindTransforms.resize(numNodes);

    hfmModel.hasSkeletonJoints = _data->skins_count > 0;
    if (hfmModel.hasSkeletonJoints) {
        std::vector<std::vector<float>> inverseBindValues;
        if (!getSkinInverseBindMatrices(inverseBindValues)) {
            qDebug(modelformat) << "GLTFSerializer::getSkinInverseBindMatrices: wrong matrices accessor type for model: "
                                << _url;
            hfmModel.loadErrorCount++;
            return false;
        }

        for (int jointIndex = 0; jointIndex < numNodes; ++jointIndex) {
            int nodeIndex = sortedNodes[jointIndex];
            auto joint = hfmModel.joints[jointIndex];

            for (size_t s = 0; s < _data->skins_count; ++s) {
                const auto& skin = _data->skins[s];
                size_t jointNodeIndex = 0;
                joint.isSkeletonJoint =
                    findNodeInPointerArray(&_data->nodes[nodeIndex], skin.joints, skin.joints_count, jointNodeIndex);

                // build inverse bind matrices
                if (joint.isSkeletonJoint) {
                    size_t matrixIndex = jointNodeIndex;
                    std::vector<float>& value = inverseBindValues[s];
                    size_t matrixCount = 16 * matrixIndex;
                    if (matrixCount + 15 >= value.size()) {
                        qDebug(modelformat)
                            << "GLTFSerializer::buildGeometry: not enough entries in jointInverseBindTransforms: " << _url;
                        hfmModel.loadErrorCount++;
                        return false;
                    }
                    jointInverseBindTransforms[jointIndex] =
                        glm::mat4(value[matrixCount], value[matrixCount + 1], value[matrixCount + 2], value[matrixCount + 3],
                                  value[matrixCount + 4], value[matrixCount + 5], value[matrixCount + 6],
                                  value[matrixCount + 7], value[matrixCount + 8], value[matrixCount + 9],
                                  value[matrixCount + 10], value[matrixCount + 11], value[matrixCount + 12],
                                  value[matrixCount + 13], value[matrixCount + 14], value[matrixCount + 15]);
                } else {
                    jointInverseBindTransforms[jointIndex] = glm::mat4();
                }
                globalBindTransforms[jointIndex] = jointInverseBindTransforms[jointIndex];
                if (joint.parentIndex != -1) {
                    globalBindTransforms[jointIndex] =
                        globalBindTransforms[joint.parentIndex] * globalBindTransforms[jointIndex];
                }
                glm::vec3 bindTranslation =
                    extractTranslation(hfmModel.offset * glm::inverse(jointInverseBindTransforms[jointIndex]));
                hfmModel.bindExtents.addPoint(bindTranslation);
            }
            hfmModel.joints[jointIndex] = joint;
        }
    }

    // Build materials
    QVector<QString> materialIDs;
    QString unknown = "Default";
    for (size_t i = 0; i < _data->materials_count; i++) {
        auto& material = _data->materials[i];
        QString mid;
        if (material.name != nullptr) {
            mid = QString(material.name);
        } else {
            mid = QString::number(i);
        }

        materialIDs.push_back(mid);
    }

    for (int i = 0; i < materialIDs.size(); ++i) {
        QString& matid = materialIDs[i];
        hfmModel.materials[matid] = HFMMaterial();
        HFMMaterial& hfmMaterial = hfmModel.materials[matid];
        hfmMaterial.name = hfmMaterial.materialID = matid;
        setHFMMaterial(hfmMaterial, _data->materials[i]);
    }

    // Build meshes
    int nodeCount = 0;
    hfmModel.meshExtents.reset();
    for (int nodeIndex : sortedNodes) {
        auto& node = _data->nodes[nodeIndex];

        if (node.mesh != nullptr) {
            hfmModel.meshes.append(HFMMesh());
            HFMMesh& mesh = hfmModel.meshes[hfmModel.meshes.size() - 1];
            mesh.modelTransform = globalTransforms[nodeIndex];

            if (!hfmModel.hasSkeletonJoints) {
                HFMCluster cluster;
                cluster.jointIndex = nodeCount;
                cluster.inverseBindMatrix = glm::mat4();
                cluster.inverseBindTransform = Transform(cluster.inverseBindMatrix);
                mesh.clusters.append(cluster);
            } else {  // skinned model
                for (int j = 0; j < numNodes; ++j) {
                    HFMCluster cluster;
                    cluster.jointIndex = j;
                    cluster.inverseBindMatrix = jointInverseBindTransforms[j];
                    cluster.inverseBindTransform = Transform(cluster.inverseBindMatrix);
                    mesh.clusters.append(cluster);
                }
            }
            HFMCluster root;
            root.jointIndex = 0;
            root.inverseBindMatrix = jointInverseBindTransforms[root.jointIndex];
            root.inverseBindTransform = Transform(root.inverseBindMatrix);
            mesh.clusters.append(root);

            QList<QString> meshAttributes;
            for (size_t primitiveIndex = 0; primitiveIndex < node.mesh->primitives_count; primitiveIndex++) {
                auto& primitive = node.mesh->primitives[primitiveIndex];
                for (size_t attributeIndex = 0; attributeIndex < primitive.attributes_count; attributeIndex++) {
                    auto& attribute = primitive.attributes[attributeIndex];
                    QString key(attribute.name);
                    if (!meshAttributes.contains(key)) {
                        meshAttributes.push_back(key);
                    }
                }
            }

            for (size_t primitiveIndex = 0; primitiveIndex < node.mesh->primitives_count; primitiveIndex++) {
                auto& primitive = node.mesh->primitives[primitiveIndex];
                HFMMeshPart part = HFMMeshPart();

                if (primitive.indices == nullptr) {
                    qDebug() << "No indices accessor for mesh: " << _url;
                    hfmModel.loadErrorCount++;
                    return false;
                }
                auto& indicesAccessor = primitive.indices;

                // Buffers
                QVector<int> indices;
                QVector<float> vertices;
                int verticesStride = 3;
                QVector<float> normals;
                int normalStride = 3;
                QVector<float> tangents;
                int tangentStride = 4;
                QVector<float> texcoords;
                int texCoordStride = 2;
                QVector<float> texcoords2;
                int texCoord2Stride = 2;
                QVector<float> colors;
                int colorStride = 3;
                QVector<uint16_t> joints;
                int jointStride = 4;
                QVector<float> weights;
                int weightStride = 4;

                indices.resize((int)indicesAccessor->count);
                size_t readIndicesCount = cgltf_accessor_unpack_indices(indicesAccessor, indices.data(), sizeof(unsigned int),
                                                                        indicesAccessor->count);

                if (readIndicesCount != indicesAccessor->count) {
                    qWarning(modelformat) << "There was a problem reading glTF INDICES data for model " << _url;
                    hfmModel.loadErrorCount++;
                    continue;
                }

                // Increment the triangle indices by the current mesh vertex count so each mesh part can all reference the same buffers within the mesh
                int prevMeshVerticesCount = mesh.vertices.count();

                // For each vertex (stride is WEIGHTS_PER_VERTEX), it contains index of the cluster that given weight belongs to.
                QVector<uint16_t> clusterJoints;
                QVector<float> clusterWeights;

                for (size_t attributeIndex = 0; attributeIndex < primitive.attributes_count; attributeIndex++) {
                    if (primitive.attributes[attributeIndex].name == nullptr) {
                        qDebug() << "Inalid accessor name for mesh: " << _url;
                        hfmModel.loadErrorCount++;
                        return false;
                    }
                    QString key(primitive.attributes[attributeIndex].name);

                    if (primitive.attributes[attributeIndex].data == nullptr) {
                        qDebug() << "Inalid accessor for mesh: " << _url;
                        hfmModel.loadErrorCount++;
                        return false;
                    }
                    auto accessor = primitive.attributes[attributeIndex].data;
                    int accessorCount = (int)accessor->count;

                    if (key == "POSITION") {
                        if (accessor->type != cgltf_type_vec3) {
                            qWarning(modelformat) << "Invalid accessor type on glTF POSITION data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        vertices.resize(accessorCount * 3);
                        size_t floatCount = cgltf_accessor_unpack_floats(accessor, vertices.data(), accessor->count * 3);
                        if (floatCount != accessor->count * 3) {
                            qWarning(modelformat) << "There was a problem reading glTF POSITION data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }
                    } else if (key == "NORMAL") {
                        if (accessor->type != cgltf_type_vec3) {
                            qWarning(modelformat) << "Invalid accessor type on glTF NORMAL data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        normals.resize(accessorCount * 3);
                        size_t floatCount = cgltf_accessor_unpack_floats(accessor, normals.data(), accessor->count * 3);
                        if (floatCount != accessor->count * 3) {
                            qWarning(modelformat) << "There was a problem reading glTF NORMAL data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }
                    } else if (key == "TANGENT") {
                        if (accessor->type == cgltf_type_vec4) {
                            tangentStride = 4;
                        } else if (accessor->type == cgltf_type_vec3) {
                            tangentStride = 3;
                        } else {
                            qWarning(modelformat) << "Invalid accessor type on glTF TANGENT data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        tangents.resize(accessorCount * tangentStride);
                        size_t floatCount =
                            cgltf_accessor_unpack_floats(accessor, tangents.data(), accessor->count * tangentStride);
                        if (floatCount != accessor->count * tangentStride) {
                            qWarning(modelformat) << "There was a problem reading glTF TANGENT data for model " << _url;
                            hfmModel.loadErrorCount++;
                            tangentStride = 0;
                            continue;
                        }
                    } else if (key == "TEXCOORD_0") {
                        if (accessor->type != cgltf_type_vec2) {
                            qWarning(modelformat) << "Invalid accessor type on glTF TEXCOORD_0 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        texcoords.resize(accessorCount * 2);
                        size_t floatCount = cgltf_accessor_unpack_floats(accessor, texcoords.data(), accessor->count * 2);
                        if (floatCount != accessor->count * 2) {
                            qWarning(modelformat) << "There was a problem reading glTF TEXCOORD_0 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }
                    } else if (key == "TEXCOORD_1") {
                        if (accessor->type != cgltf_type_vec2) {
                            qWarning(modelformat) << "Invalid accessor type on glTF TEXCOORD_1 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        texcoords2.resize(accessorCount * 2);
                        size_t floatCount = cgltf_accessor_unpack_floats(accessor, texcoords2.data(), accessor->count * 2);
                        if (floatCount != accessor->count * 2) {
                            qWarning(modelformat) << "There was a problem reading glTF TEXCOORD_1 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }
                    } else if (key == "COLOR_0") {
                        if (accessor->type == cgltf_type_vec4) {
                            colorStride = 4;
                        } else if (accessor->type == cgltf_type_vec3) {
                            colorStride = 3;
                        } else {
                            qWarning(modelformat) << "Invalid accessor type on glTF COLOR_0 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        colors.resize(accessorCount * colorStride);
                        size_t floatCount =
                            cgltf_accessor_unpack_floats(accessor, colors.data(), accessor->count * colorStride);
                        if (floatCount != accessor->count * colorStride) {
                            qWarning(modelformat) << "There was a problem reading glTF COLOR_0 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }
                    } else if (key == "JOINTS_0") {
                        if (accessor->type == cgltf_type_vec4) {
                            jointStride = 4;
                        } else if (accessor->type == cgltf_type_vec3) {
                            jointStride = 3;
                        } else if (accessor->type == cgltf_type_vec2) {
                            jointStride = 2;
                        } else if (accessor->type == cgltf_type_scalar) {
                            jointStride = 1;
                        } else {
                            qWarning(modelformat) << "Invalid accessor type on glTF JOINTS_0 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        joints.resize(accessorCount * jointStride);
                        cgltf_uint jointIndices[4];
                        for (size_t i = 0; i < accessor->count; i++) {
                            cgltf_accessor_read_uint(accessor, i, jointIndices, jointStride);
                            for (int component = 0; component < jointStride; component++) {
                                joints[(int)i * jointStride + component] = (uint16_t)jointIndices[component];
                            }
                        }

                    } else if (key == "WEIGHTS_0") {
                        if (accessor->type == cgltf_type_vec4) {
                            weightStride = 4;
                        } else if (accessor->type == cgltf_type_vec3) {
                            weightStride = 3;
                        } else if (accessor->type == cgltf_type_vec2) {
                            weightStride = 2;
                        } else if (accessor->type == cgltf_type_scalar) {
                            weightStride = 1;
                        } else {
                            qWarning(modelformat) << "Invalid accessor type on glTF WEIGHTS_0 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        weights.resize(accessorCount * weightStride);
                        size_t floatCount =
                            cgltf_accessor_unpack_floats(accessor, weights.data(), accessor->count * weightStride);
                        if (floatCount != accessor->count * weightStride) {
                            qWarning(modelformat) << "There was a problem reading glTF WEIGHTS_0 data for model " << _url;
                            hfmModel.loadErrorCount++;
                            continue;
                        }
                    }
                }

                // Validation stage
                if (indices.count() == 0) {
                    qWarning(modelformat) << "Missing indices for model " << _url;
                    hfmModel.loadErrorCount++;
                    continue;
                }
                if (vertices.count() == 0) {
                    qWarning(modelformat) << "Missing vertices for model " << _url;
                    hfmModel.loadErrorCount++;
                    continue;
                }

                int partVerticesCount = vertices.size() / 3;

                // generate the normals if they don't exist
                if (normals.size() == 0) {
                    QVector<int> newIndices;
                    QVector<float> newVertices;
                    QVector<float> newNormals;
                    QVector<float> newTexcoords;
                    QVector<float> newTexcoords2;
                    QVector<float> newColors;
                    QVector<uint16_t> newJoints;
                    QVector<float> newWeights;

                    for (int n = 0; n + 2 < indices.size(); n = n + 3) {
                        int v1_index = (indices[n + 0] * 3);
                        int v2_index = (indices[n + 1] * 3);
                        int v3_index = (indices[n + 2] * 3);

                        if (v1_index + 2 >= vertices.size() || v2_index + 2 >= vertices.size() ||
                            v3_index + 2 >= vertices.size()) {
                            qWarning(modelformat) << "Indices out of range for model " << _url;
                            hfmModel.loadErrorCount++;
                            return false;
                        }

                        glm::vec3 v1 = glm::vec3(vertices[v1_index], vertices[v1_index + 1], vertices[v1_index + 2]);
                        glm::vec3 v2 = glm::vec3(vertices[v2_index], vertices[v2_index + 1], vertices[v2_index + 2]);
                        glm::vec3 v3 = glm::vec3(vertices[v3_index], vertices[v3_index + 1], vertices[v3_index + 2]);

                        newVertices.append(v1.x);
                        newVertices.append(v1.y);
                        newVertices.append(v1.z);
                        newVertices.append(v2.x);
                        newVertices.append(v2.y);
                        newVertices.append(v2.z);
                        newVertices.append(v3.x);
                        newVertices.append(v3.y);
                        newVertices.append(v3.z);

                        glm::vec3 norm = glm::normalize(glm::cross(v2 - v1, v3 - v1));

                        newNormals.append(norm.x);
                        newNormals.append(norm.y);
                        newNormals.append(norm.z);
                        newNormals.append(norm.x);
                        newNormals.append(norm.y);
                        newNormals.append(norm.z);
                        newNormals.append(norm.x);
                        newNormals.append(norm.y);
                        newNormals.append(norm.z);

                        if (texcoords.size() == partVerticesCount * texCoordStride) {
                            GLTF_APPEND_ARRAY_2(newTexcoords, texcoords)
                        }

                        if (texcoords2.size() == partVerticesCount * texCoord2Stride) {
                            GLTF_APPEND_ARRAY_2(newTexcoords2, texcoords2)
                        }

                        if (colors.size() == partVerticesCount * colorStride) {
                            if (colorStride == 4) {
                                GLTF_APPEND_ARRAY_4(newColors, colors)
                            } else {
                                GLTF_APPEND_ARRAY_3(newColors, colors)
                            }
                        }

                        if (joints.size() == partVerticesCount * jointStride) {
                            if (jointStride == 4) {
                                GLTF_APPEND_ARRAY_4(newJoints, joints)
                            } else if (jointStride == 3) {
                                GLTF_APPEND_ARRAY_3(newJoints, joints)
                            } else if (jointStride == 2) {
                                GLTF_APPEND_ARRAY_2(newJoints, joints)
                            } else {
                                GLTF_APPEND_ARRAY_1(newJoints, joints)
                            }
                        }

                        if (weights.size() == partVerticesCount * weightStride) {
                            if (weightStride == 4) {
                                GLTF_APPEND_ARRAY_4(newWeights, weights)
                            } else if (weightStride == 3) {
                                GLTF_APPEND_ARRAY_3(newWeights, weights)
                            } else if (weightStride == 2) {
                                GLTF_APPEND_ARRAY_2(newWeights, weights)
                            } else {
                                GLTF_APPEND_ARRAY_1(newWeights, weights)
                            }
                        }
                        newIndices.append(n);
                        newIndices.append(n + 1);
                        newIndices.append(n + 2);
                    }

                    vertices = newVertices;
                    normals = newNormals;
                    tangents = QVector<float>();
                    texcoords = newTexcoords;
                    texcoords2 = newTexcoords2;
                    colors = newColors;
                    joints = newJoints;
                    weights = newWeights;
                    indices = newIndices;

                    partVerticesCount = vertices.size() / 3;
                }

                QVector<int> validatedIndices;
                for (int n = 0; n < indices.count(); ++n) {
                    if (indices[n] < partVerticesCount) {
                        validatedIndices.push_back(indices[n] + prevMeshVerticesCount);
                    } else {
                        validatedIndices = QVector<int>();
                        break;
                    }
                }

                if (validatedIndices.size() == 0) {
                    qWarning(modelformat) << "No valid indices for model " << _url;
                    hfmModel.loadErrorCount++;
                    continue;
                }

                part.triangleIndices.append(validatedIndices);

                for (int n = 0; n + verticesStride - 1 < vertices.size(); n = n + verticesStride) {
                    mesh.vertices.push_back(glm::vec3(vertices[n], vertices[n + 1], vertices[n + 2]));
                }

                for (int n = 0; n + normalStride - 1 < normals.size(); n = n + normalStride) {
                    mesh.normals.push_back(glm::vec3(normals[n], normals[n + 1], normals[n + 2]));
                }

                // TODO: add correct tangent generation
                if (tangents.size() == partVerticesCount * tangentStride) {
                    for (int n = 0; n + tangentStride - 1 < tangents.size(); n += tangentStride) {
                        float tanW = tangentStride == 4 ? tangents[n + 3] : 1;
                        mesh.tangents.push_back(glm::vec3(tanW * tangents[n], tangents[n + 1], tanW * tangents[n + 2]));
                    }
                } else {
                    if (meshAttributes.contains("TANGENT")) {
                        for (int i = 0; i < partVerticesCount; ++i) {
                            mesh.tangents.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
                        }
                    }
                }

                if (texcoords.size() == partVerticesCount * texCoordStride) {
                    for (int n = 0; n + 1 < texcoords.size(); n = n + 2) {
                        mesh.texCoords.push_back(glm::vec2(texcoords[n], texcoords[n + 1]));
                    }
                } else {
                    if (meshAttributes.contains("TEXCOORD_0")) {
                        for (int i = 0; i < partVerticesCount; ++i) {
                            mesh.texCoords.push_back(glm::vec2(0.0f, 0.0f));
                        }
                    }
                }

                if (texcoords2.size() == partVerticesCount * texCoord2Stride) {
                    for (int n = 0; n + 1 < texcoords2.size(); n = n + 2) {
                        mesh.texCoords1.push_back(glm::vec2(texcoords2[n], texcoords2[n + 1]));
                    }
                } else {
                    if (meshAttributes.contains("TEXCOORD_1")) {
                        for (int i = 0; i < partVerticesCount; ++i) {
                            mesh.texCoords1.push_back(glm::vec2(0.0f, 0.0f));
                        }
                    }
                }

                if (colors.size() == partVerticesCount * colorStride) {
                    for (int n = 0; n + 2 < colors.size(); n += colorStride) {
                        mesh.colors.push_back(ColorUtils::tosRGBVec3(glm::vec3(colors[n], colors[n + 1], colors[n + 2])));
                    }
                } else {
                    if (meshAttributes.contains("COLOR_0")) {
                        for (int i = 0; i < partVerticesCount; ++i) {
                            mesh.colors.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
                        }
                    }
                }

                if (joints.size() == partVerticesCount * jointStride) {
                    for (int n = 0; n < joints.size(); n += jointStride) {
                        clusterJoints.push_back(joints[n]);
                        if (jointStride > 1) {
                            clusterJoints.push_back(joints[n + 1]);
                            if (jointStride > 2) {
                                clusterJoints.push_back(joints[n + 2]);
                                if (jointStride > 3) {
                                    clusterJoints.push_back(joints[n + 3]);
                                } else {
                                    clusterJoints.push_back(0);
                                }
                            } else {
                                clusterJoints.push_back(0);
                                clusterJoints.push_back(0);
                            }
                        } else {
                            clusterJoints.push_back(0);
                            clusterJoints.push_back(0);
                            clusterJoints.push_back(0);
                        }
                    }
                } else {
                    if (meshAttributes.contains("JOINTS_0")) {
                        for (int i = 0; i < partVerticesCount; ++i) {
                            for (int j = 0; j < 4; ++j) {
                                clusterJoints.push_back(0);
                            }
                        }
                    }
                }

                if (weights.size() == partVerticesCount * weightStride) {
                    for (int n = 0; n + weightStride - 1 < weights.size(); n += weightStride) {
                        clusterWeights.push_back(weights[n]);
                        if (weightStride > 1) {
                            clusterWeights.push_back(weights[n + 1]);
                            if (weightStride > 2) {
                                clusterWeights.push_back(weights[n + 2]);
                                if (weightStride > 3) {
                                    clusterWeights.push_back(weights[n + 3]);
                                } else {
                                    clusterWeights.push_back(0.0f);
                                }
                            } else {
                                clusterWeights.push_back(0.0f);
                                clusterWeights.push_back(0.0f);
                            }
                        } else {
                            clusterWeights.push_back(0.0f);
                            clusterWeights.push_back(0.0f);
                            clusterWeights.push_back(0.0f);
                        }
                    }
                } else {
                    if (meshAttributes.contains("WEIGHTS_0")) {
                        for (int i = 0; i < partVerticesCount; ++i) {
                            clusterWeights.push_back(1.0f);
                            for (int j = 1; j < 4; ++j) {
                                clusterWeights.push_back(0.0f);
                            }
                        }
                    }
                }

                // Build weights (adapted from FBXSerializer.cpp)
                if (hfmModel.hasSkeletonJoints) {
                    int prevMeshClusterIndexCount = mesh.clusterIndices.count();
                    int prevMeshClusterWeightCount = mesh.clusterWeights.count();
                    const int WEIGHTS_PER_VERTEX = 4;
                    const float ALMOST_HALF = 0.499f;
                    int numVertices = mesh.vertices.size() - prevMeshVerticesCount;

                    // Append new cluster indices and weights for this mesh part
                    for (int i = 0; i < numVertices * WEIGHTS_PER_VERTEX; ++i) {
                        mesh.clusterIndices.push_back(mesh.clusters.size() - 1);
                        mesh.clusterWeights.push_back(0);
                    }

                    for (int c = 0; c < clusterJoints.size(); ++c) {
                        if (mesh.clusterIndices.length() <= prevMeshClusterIndexCount + c) {
                            qCWarning(modelformat)
                                << "Trying to write past end of clusterIndices at" << prevMeshClusterIndexCount + c;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        if (clusterJoints.length() <= c) {
                            qCWarning(modelformat) << "Trying to read past end of clusterJoints at" << c;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        if (node.skin->joints_count <= clusterJoints[c]) {
                            qCWarning(modelformat)
                                << "Trying to read past end of _file.skins[node.skin].joints at" << clusterJoints[c]
                                << "; there are only" << node.skin->joints_count << "for skin" << node.skin->name;
                            hfmModel.loadErrorCount++;
                            continue;
                        }

                        size_t jointIndex = 0;
                        if (!findPointerInArray(node.skin->joints[clusterJoints[c]], _data->nodes, _data->nodes_count,
                                                jointIndex)) {
                            qCWarning(modelformat)
                                << "Cannot find the joint " << node.skin->joints[clusterJoints[c]]->name << " in joint array";
                            hfmModel.loadErrorCount++;
                            continue;
                        }
                        mesh.clusterIndices[prevMeshClusterIndexCount + c] = originalToNewNodeIndexMap[(int)jointIndex];
                    }

                    // normalize and compress to 16-bits
                    for (int i = 0; i < numVertices; ++i) {
                        int j = i * WEIGHTS_PER_VERTEX;

                        float totalWeight = 0.0f;
                        for (int k = j; k < j + WEIGHTS_PER_VERTEX; ++k) {
                            totalWeight += clusterWeights[k];
                        }
                        if (totalWeight > 0.0f) {
                            float weightScalingFactor = (float)(UINT16_MAX) / totalWeight;
                            for (int k = j; k < j + WEIGHTS_PER_VERTEX; ++k) {
                                mesh.clusterWeights[prevMeshClusterWeightCount + k] =
                                    (uint16_t)(weightScalingFactor * clusterWeights[k] + ALMOST_HALF);
                            }
                        } else {
                            mesh.clusterWeights[prevMeshClusterWeightCount + j] = (uint16_t)((float)(UINT16_MAX) + ALMOST_HALF);
                        }
                        for (int k = j; k < j + WEIGHTS_PER_VERTEX; ++k) {
                            int clusterIndex = mesh.clusterIndices[prevMeshClusterIndexCount + k];
                            ShapeVertices& points = hfmModel.shapeVertices.at(clusterIndex);
                            glm::vec3 globalMeshScale = extractScale(globalTransforms[nodeIndex]);
                            const glm::mat4 meshToJoint =
                                glm::scale(glm::mat4(), globalMeshScale) * jointInverseBindTransforms[clusterIndex];

                            const uint16_t EXPANSION_WEIGHT_THRESHOLD = UINT16_MAX / 4;  // Equivalent of 0.25f?
                            if (mesh.clusterWeights[prevMeshClusterWeightCount + k] >= EXPANSION_WEIGHT_THRESHOLD) {
                                auto& vertex = mesh.vertices[prevMeshVerticesCount + i];
                                const glm::mat4 vertexTransform = meshToJoint * glm::translate(vertex);
                                glm::vec3 transformedVertex = extractTranslation(vertexTransform);
                                points.push_back(transformedVertex);
                            }
                        }
                    }
                }

                size_t materialIndex = 0;
                if (primitive.material != nullptr &&
                    !findPointerInArray(primitive.material, _data->materials, _data->materials_count, materialIndex)) {
                    qCWarning(modelformat) << "GLTFSerializer::buildGeometry: Invalid material pointer";
                    hfmModel.loadErrorCount++;
                    return false;
                }
                if (primitive.material != nullptr) {
                    part.materialID = materialIDs[(int)materialIndex];
                }
                mesh.parts.push_back(part);

                // populate the texture coordinates if they don't exist
                if (mesh.texCoords.size() == 0 && !hfmModel.hasSkeletonJoints) {
                    for (int i = 0; i < part.triangleIndices.size(); ++i) {
                        mesh.texCoords.push_back(glm::vec2(0.0, 1.0));
                    }
                }

                // Build morph targets (blend shapes)
                if (primitive.targets_count) {
                    // Build list of blendshapes from FST and model.
                    typedef QPair<int, float> WeightedIndex;
                    hifi::VariantMultiHash blendshapeMappings = mapping.value("bs").toHash();
                    QMultiHash<QString, WeightedIndex> blendshapeIndices;
                    for (int i = 0;; ++i) {
                        auto blendshapeName = QString(BLENDSHAPE_NAMES[i]);
                        if (blendshapeName.isEmpty()) {
                            break;
                        }
                        auto mappings = blendshapeMappings.values(blendshapeName);
                        if (mappings.count() > 0) {
                            // Use blendshape from mapping.
                            foreach (const QVariant& mappingVariant, mappings) {
                                auto blendshapeMapping = mappingVariant.toList();
                                blendshapeIndices.insert(blendshapeMapping.at(0).toString(),
                                                         WeightedIndex(i, blendshapeMapping.at(1).toFloat()));
                            }
                        } else {
                            // Use blendshape from model.
                            std::string blendshapeNameString = blendshapeName.toStdString();
                            for (size_t j = 0; j < node.mesh->target_names_count; j++) {
                                if (strcmp(node.mesh->target_names[j], blendshapeNameString.c_str()) == 0) {
                                    blendshapeIndices.insert(blendshapeName, WeightedIndex(i, 1.0f));
                                    break;
                                }
                            }
                        }
                    }

                    // If an FST isn't being used and the model is likely from ReadyPlayerMe, add blendshape synonyms.
                    QVector<QString> fileTargetNames;
                    fileTargetNames.reserve((int)node.mesh->target_names_count);
                    for (size_t i = 0; i < node.mesh->target_names_count; i++) {
                        fileTargetNames.push_back(QString(node.mesh->target_names[i]));
                    }

                    bool likelyReadyPlayerMeFile =
                        fileTargetNames.contains("browOuterUpLeft") && fileTargetNames.contains("browInnerUp") &&
                        fileTargetNames.contains("browDownLeft") && fileTargetNames.contains("eyeBlinkLeft") &&
                        fileTargetNames.contains("eyeWideLeft") && fileTargetNames.contains("mouthLeft") &&
                        fileTargetNames.contains("viseme_O") && fileTargetNames.contains("mouthShrugLower");
                    if (blendshapeMappings.count() == 0 && likelyReadyPlayerMeFile) {
                        QHash<QString, QPair<QString, float>>::const_iterator synonym =
                            READYPLAYERME_BLENDSHAPES_MAP.constBegin();
                        while (synonym != READYPLAYERME_BLENDSHAPES_MAP.constEnd()) {
                            if (fileTargetNames.contains(synonym.key())) {
                                auto blendshape = BLENDSHAPE_LOOKUP_MAP.find(synonym.value().first);
                                if (blendshape != BLENDSHAPE_LOOKUP_MAP.end()) {
                                    blendshapeIndices.insert(synonym.key(),
                                                             WeightedIndex(blendshape.value(), synonym.value().second));
                                }
                            }
                            ++synonym;
                        }
                    }

                    // Create blendshapes.
                    if (!blendshapeIndices.isEmpty()) {
                        mesh.blendshapes.resize((int)Blendshapes::BlendshapeCount);
                    }
                    auto keys = blendshapeIndices.keys();
                    auto values = blendshapeIndices.values();
                    QVector<QString> names;
                    names.reserve((int)node.mesh->target_names_count);
                    for (size_t i = 0; i < node.mesh->target_names_count; i++) {
                        names.push_back(QString(node.mesh->target_names[i]));
                    }

                    for (int weightedIndex = 0; weightedIndex < keys.size(); ++weightedIndex) {
                        float weight = 1.0f;
                        int indexFromMapping = weightedIndex;
                        int targetIndex = weightedIndex;
                        hfmModel.blendshapeChannelNames.push_back("target_" + QString::number(weightedIndex));

                        if (!names.isEmpty()) {
                            targetIndex = names.indexOf(keys[weightedIndex]);
                            if (targetIndex == -1) {
                                continue;  // Ignore blendshape targets not present in glTF file.
                            }
                            indexFromMapping = values[weightedIndex].first;
                            weight = values[weightedIndex].second;
                            hfmModel.blendshapeChannelNames[weightedIndex] = keys[weightedIndex];
                        }

                        HFMBlendshape& blendshape = mesh.blendshapes[indexFromMapping];
                        auto target = primitive.targets[targetIndex];

                        QVector<glm::vec3> normals;
                        QVector<glm::vec3> vertices;

                        size_t normalAttributeIndex = 0;
                        if (findAttribute("NORMAL", target.attributes, target.attributes_count, normalAttributeIndex)) {
                            if (!generateTargetData(target.attributes[normalAttributeIndex].data, weight, normals)) {
                                qWarning(modelformat)
                                    << "Invalid NORMAL accessor on generateTargetData vertices for model " << _url;
                                hfmModel.loadErrorCount++;
                                return false;
                            }
                        }
                        size_t positionAttributeIndex = 0;
                        if (findAttribute("POSITION", target.attributes, target.attributes_count, positionAttributeIndex)) {
                            if (!generateTargetData(target.attributes[positionAttributeIndex].data, weight, vertices)) {
                                qWarning(modelformat)
                                    << "Invalid POSITION accessor on generateTargetData vertices for model " << _url;
                                hfmModel.loadErrorCount++;
                                return false;
                            }
                        }

                        if (blendshape.indices.size() < prevMeshVerticesCount + vertices.size()) {
                            blendshape.indices.resize(prevMeshVerticesCount + vertices.size());
                            blendshape.vertices.resize(prevMeshVerticesCount + vertices.size());
                            blendshape.normals.resize(prevMeshVerticesCount + vertices.size());
                        }
                        //TODO: it looks like this can support sparse encoding, since there are indices?
                        for (int i = 0; i < vertices.size(); i++) {
                            blendshape.indices[prevMeshVerticesCount + i] = prevMeshVerticesCount + i;
                            blendshape.vertices[prevMeshVerticesCount + i] += vertices.value(i);
                            // Prevent out-of-bounds access if blendshape normals are not available
                            if (i < normals.size()) {
                                blendshape.normals[prevMeshVerticesCount + i] += normals.value(i);
                            } else {
                                if (prevMeshVerticesCount + i < mesh.normals.size()) {
                                    blendshape.normals[prevMeshVerticesCount + i] = mesh.normals[prevMeshVerticesCount + i];
                                } else {
                                    qWarning(modelformat) << "Blendshape has more vertices than original mesh " << _url;
                                    hfmModel.loadErrorCount++;
                                    return false;
                                }
                            }
                        }
                    }
                }

                foreach (const glm::vec3& vertex, mesh.vertices) {
                    glm::vec3 transformedVertex = glm::vec3(globalTransforms[nodeIndex] * glm::vec4(vertex, 1.0f));
                    mesh.meshExtents.addPoint(transformedVertex);
                    hfmModel.meshExtents.addPoint(transformedVertex);
                }
            }

            // Mesh extents must be at least a minimum size, in particular for blendshapes to work on planar meshes.
            const float MODEL_MIN_DIMENSION = 0.001f;
            auto delta = glm::max(glm::vec3(MODEL_MIN_DIMENSION) - mesh.meshExtents.size(), glm::vec3(0.0f)) / 2.0f;
            mesh.meshExtents.minimum -= delta;
            mesh.meshExtents.maximum += delta;
            hfmModel.meshExtents.minimum -= delta;
            hfmModel.meshExtents.maximum += delta;

            mesh.meshIndex = hfmModel.meshes.size();
        }
        ++nodeCount;
    }

    return true;
}