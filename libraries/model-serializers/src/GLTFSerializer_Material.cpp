//
//  GLTFSerializer_Material.cpp
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


void GLTFSerializer::setHFMMaterial(HFMMaterial& hfmMat, const cgltf_material& material) {
    hfmMat._material = std::make_shared<graphics::Material>();
    for (size_t i = 0; i < material.extensions_count; i++) {
        auto& extension = material.extensions[i];
        if (extension.name != nullptr) {
            if (strcmp(extension.name, "VRMC_materials_mtoon") == 0 && extension.data != nullptr) {
                hfmMat.isMToonMaterial = true;
                auto mToonMaterial = std::make_shared<NetworkMToonMaterial>();
                QJsonDocument mToonExtension = QJsonDocument::fromJson(extension.data);
                if (!mToonExtension.isNull()) {
                    if (mToonExtension["shadeColorFactor"].isArray()) {
                        auto array = mToonExtension["shadeColorFactor"].toArray();
                        glm::vec3 shadeLinear = glm::vec3(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
                        glm::vec3 shade = ColorUtils::tosRGBVec3(shadeLinear);
                        mToonMaterial->setShade(shade);
                    }
                    if (mToonExtension["shadeMultiplyTexture"].isObject()) {
                        QJsonObject object = mToonExtension["shadeMultiplyTexture"].toObject();
                        if (object["index"].isDouble() && object["index"].toInt() < (int)_data->textures_count) {
                            hfmMat.shadeTexture = getHFMTexture(&_data->textures[object["index"].toInt()]);
                        }
                    }
                    if (mToonExtension["shadingShiftFactor"].isDouble()) {
                        mToonMaterial->setShadingShift(mToonExtension["shadingShiftFactor"].toDouble());
                    }
                    if (mToonExtension["shadingShiftTexture"].isObject()) {
                        QJsonObject object = mToonExtension["shadingShiftTexture"].toObject();
                        if (object["index"].isDouble() && object["index"].toInt() < (int)_data->textures_count) {
                            hfmMat.shadingShiftTexture = getHFMTexture(&_data->textures[object["index"].toInt()]);
                        }
                    }
                    if (mToonExtension["shadingToonyFactor"].isDouble()) {
                        mToonMaterial->setShadingToony(mToonExtension["shadingToonyFactor"].toDouble());
                    }
                    if (mToonExtension["matcapFactor"].isArray()) {
                        auto array = mToonExtension["matcapFactor"].toArray();
                        glm::vec3 matcapLinear = glm::vec3(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
                        glm::vec3 matcap = ColorUtils::tosRGBVec3(matcapLinear);
                        mToonMaterial->setMatcap(matcap);
                    }
                    if (mToonExtension["matcapTexture"].isObject()) {
                        QJsonObject object = mToonExtension["matcapTexture"].toObject();
                        if (object["index"].isDouble() && object["index"].toInt() < (int)_data->textures_count) {
                            hfmMat.matcapTexture = getHFMTexture(&_data->textures[object["index"].toInt()]);
                        }
                    }
                    if (mToonExtension["parametricRimColorFactor"].isArray()) {
                        auto array = mToonExtension["parametricRimColorFactor"].toArray();
                        glm::vec3 parametricRimLinear =
                            glm::vec3(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
                        glm::vec3 parametricRim = ColorUtils::tosRGBVec3(parametricRimLinear);
                        mToonMaterial->setParametricRim(parametricRim);
                    }
                    if (mToonExtension["parametricRimFresnelPowerFactor"].isDouble()) {
                        mToonMaterial->setParametricRimFresnelPower(
                            mToonExtension["parametricRimFresnelPowerFactor"].toDouble());
                    }
                    if (mToonExtension["parametricRimLiftFactor"].isDouble()) {
                        mToonMaterial->setParametricRimLift(mToonExtension["parametricRimLiftFactor"].toDouble());
                    }
                    if (mToonExtension["rimMultiplyTexture"].isObject()) {
                        QJsonObject object = mToonExtension["rimMultiplyTexture"].toObject();
                        if (object["index"].isDouble() && object["index"].toInt() < (int)_data->textures_count) {
                            hfmMat.rimTexture = getHFMTexture(&_data->textures[object["index"].toInt()]);
                        }
                    }
                    if (mToonExtension["rimLightingMixFactor"].isDouble()) {
                        mToonMaterial->setRimLightingMix(mToonExtension["rimLightingMixFactor"].toDouble());
                    }
                    // FIXME: Outlines are currently disabled because they're buggy
                    //if (mToonExtension["outlineWidthMode"].isString()) {
                    //    QString outlineWidthMode = mToonExtension["outlineWidthMode"].toString();
                    //    if (outlineWidthMode == "none") {
                    //        mToonMaterial->setOutlineWidthMode(NetworkMToonMaterial::OutlineWidthMode::OUTLINE_NONE);
                    //    } else if (outlineWidthMode == "worldCoordinates") {
                    //        mToonMaterial->setOutlineWidthMode(NetworkMToonMaterial::OutlineWidthMode::OUTLINE_WORLD);
                    //    } else if (outlineWidthMode == "screenCoordinates") {
                    //        mToonMaterial->setOutlineWidthMode(NetworkMToonMaterial::OutlineWidthMode::OUTLINE_SCREEN);
                    //    }
                    //}
                    if (mToonExtension["outlineWidthFactor"].isDouble()) {
                        mToonMaterial->setOutlineWidth(mToonExtension["outlineWidthFactor"].toDouble());
                    }
                    if (mToonExtension["outlineColorFactor"].isArray()) {
                        auto array = mToonExtension["outlineColorFactor"].toArray();
                        glm::vec3 outlineLinear = glm::vec3(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
                        glm::vec3 outline = ColorUtils::tosRGBVec3(outlineLinear);
                        mToonMaterial->setOutline(outline);
                    }
                    if (mToonExtension["uvAnimationMaskTexture"].isObject()) {
                        QJsonObject object = mToonExtension["uvAnimationMaskTexture"].toObject();
                        if (object["index"].isDouble() && object["index"].toInt() < (int)_data->textures_count) {
                            hfmMat.uvAnimationTexture = getHFMTexture(&_data->textures[object["index"].toInt()]);
                        }
                    }
                    if (mToonExtension["uvAnimationScrollXSpeedFactor"].isDouble()) {
                        mToonMaterial->setUVAnimationScrollXSpeed(mToonExtension["uvAnimationScrollXSpeedFactor"].toDouble());
                    }
                    if (mToonExtension["uvAnimationScrollYSpeedFactor"].isDouble()) {
                        mToonMaterial->setUVAnimationScrollYSpeed(mToonExtension["uvAnimationScrollYSpeedFactor"].toDouble());
                    }
                    if (mToonExtension["uvAnimationRotationSpeedFactor"].isDouble()) {
                        mToonMaterial->setUVAnimationRotationSpeed(mToonExtension["uvAnimationRotationSpeedFactor"].toDouble());
                    }
                }
                hfmMat._material = mToonMaterial;
            }
        }
    }

    if (material.alpha_mode == cgltf_alpha_mode_opaque) {
        hfmMat._material->setOpacityMapMode(graphics::MaterialKey::OPACITY_MAP_OPAQUE);
    } else if (material.alpha_mode == cgltf_alpha_mode_mask) {
        hfmMat._material->setOpacityMapMode(graphics::MaterialKey::OPACITY_MAP_MASK);
    } else if (material.alpha_mode == cgltf_alpha_mode_blend) {
        hfmMat._material->setOpacityMapMode(graphics::MaterialKey::OPACITY_MAP_BLEND);
    } else {
        hfmMat._material->setOpacityMapMode(graphics::MaterialKey::OPACITY_MAP_OPAQUE);  // GLTF defaults to opaque
    }

    hfmMat._material->setOpacityCutoff(material.alpha_cutoff);

    // VRMC_materials_mtoon takes precedence over KHR_materials_unlit
    if (!hfmMat.isMToonMaterial) {
        hfmMat._material->setUnlit(material.unlit);
    }

    if (material.double_sided) {
        hfmMat._material->setCullFaceMode(graphics::MaterialKey::CullFaceMode::CULL_NONE);
    }

    glm::vec3 emissiveLinear = glm::vec3(material.emissive_factor[0], material.emissive_factor[1], material.emissive_factor[2]);
    glm::vec3 emissive = ColorUtils::tosRGBVec3(emissiveLinear);
    hfmMat._material->setEmissive(emissive);

    if (material.emissive_texture.texture != nullptr) {
        hfmMat.emissiveTexture = getHFMTexture(material.emissive_texture.texture);
        hfmMat.useEmissiveMap = true;
    }

    if (material.normal_texture.texture != nullptr) {
        hfmMat.normalTexture = getHFMTexture(material.normal_texture.texture);
        hfmMat.useNormalMap = true;
    }

    if (material.occlusion_texture.texture != nullptr) {
        hfmMat.occlusionTexture = getHFMTexture(material.occlusion_texture.texture);
        hfmMat.useOcclusionMap = true;
    }

    if (material.has_pbr_metallic_roughness) {
        hfmMat.isPBSMaterial = true;

        hfmMat.metallic = material.pbr_metallic_roughness.metallic_factor;
        hfmMat._material->setMetallic(hfmMat.metallic);

        if (material.pbr_metallic_roughness.base_color_texture.texture != nullptr) {
            hfmMat.opacityTexture = getHFMTexture(material.pbr_metallic_roughness.base_color_texture.texture);
            hfmMat.albedoTexture = getHFMTexture(material.pbr_metallic_roughness.base_color_texture.texture);
            hfmMat.useAlbedoMap = true;
        }
        if (material.pbr_metallic_roughness.metallic_roughness_texture.texture) {
            hfmMat.roughnessTexture = getHFMTexture(material.pbr_metallic_roughness.metallic_roughness_texture.texture);
            hfmMat.roughnessTexture.sourceChannel = image::ColorChannel::GREEN;
            hfmMat.useRoughnessMap = true;
            hfmMat.metallicTexture = getHFMTexture(material.pbr_metallic_roughness.metallic_roughness_texture.texture);
            hfmMat.metallicTexture.sourceChannel = image::ColorChannel::BLUE;
            hfmMat.useMetallicMap = true;
        }

        hfmMat._material->setRoughness(material.pbr_metallic_roughness.roughness_factor);

        glm::vec3 lcolor = glm::vec3(material.pbr_metallic_roughness.base_color_factor[0],
                                     material.pbr_metallic_roughness.base_color_factor[1],
                                     material.pbr_metallic_roughness.base_color_factor[2]);
        glm::vec3 dcolor = ColorUtils::tosRGBVec3(lcolor);
        hfmMat.diffuseColor = dcolor;
        hfmMat._material->setAlbedo(dcolor);
        hfmMat._material->setOpacity(material.pbr_metallic_roughness.base_color_factor[3]);
    }
}

HFMTexture GLTFSerializer::getHFMTexture(const cgltf_texture* texture) {
    HFMTexture hfmTex = HFMTexture();
    hfmTex.texcoordSet = 0;

    auto image = texture->image;

    // Check for WebP extension
    for (size_t i = 0; i < texture->extensions_count; i++) {
        auto& extension = texture->extensions[i];
        if (extension.name != nullptr && strcmp(extension.name, "EXT_texture_webp") == 0 && extension.data != nullptr) {
            QJsonDocument webPExtension = QJsonDocument::fromJson(extension.data);
            if (!webPExtension.isNull() && webPExtension["source"].isDouble()) {
                int imageIndex = webPExtension["source"].isDouble();
                if (imageIndex > 0 && (size_t)imageIndex < _data->images_count) {
                    image = &_data->images[(int)(webPExtension["source"].toDouble())];
                    break;
                }
            }
        }
    }

    if (image) {
        QString url = image->uri;

        QString fileName = hifi::URL(url).fileName();
        hifi::URL textureUrl = _url.resolved(url);
        hfmTex.name = fileName;
        hfmTex.filename = textureUrl.toEncoded();

        if (_url.path().endsWith("glb")) {
            cgltf_buffer_view* bufferView = image->buffer_view;

            size_t offset = bufferView->offset;
            int length = (int)bufferView->size;

            size_t imageIndex = 0;
            if (!findPointerInArray(image, _data->images, _data->images_count, imageIndex)) {
                // This should never happen. It would mean a bug in cgltf library.
                qDebug(modelformat) << "GLTFSerializer::getHFMTexture: can't find texture in the array";
                return hfmTex;
            }

            if (offset + length > bufferView->buffer->size) {
                qDebug(modelformat) << "GLTFSerializer::getHFMTexture: texture data to short";
                return hfmTex;
            }
            hfmTex.content = QByteArray(static_cast<const char*>(bufferView->buffer->data) + offset, length);
            hfmTex.filename = textureUrl.toEncoded().append(QString::number(imageIndex).toUtf8());
        }

        if (url.contains("data:image/jpeg;base64,") || url.contains("data:image/png;base64,") ||
            url.contains("data:image/webp;base64,")) {
            hfmTex.content = requestEmbeddedData(url);
        }
    }
    return hfmTex;
}