//
//  VRMTests.cpp
//  tests/model-serializers/src
//
//  Created by Dale Glass on 20/11/2022.
//  Copyright 2022 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html


// This test checks a large amount of files. To debug more comfortably and avoid going through
// a lot of uninteresting data, QTest allows us to narrow down what gets run with command line
// arguments, like this:
//
//     ./model-serializers-VRMTests loadGLTF:gltf2.0-RecursiveSkeletons.glb
//
// This will run only the loadGLTF test, and only on the gltf2.0-RecursiveSkeletons.glb file.

#include "VRMTests.h"
#include "GLTFSerializer.h"
#include "FBXSerializer.h"
#include "OBJSerializer.h"

#include "Gzip.h"
#include "model-networking/ModelLoader.h"
#include <hfm/ModelFormatRegistry.h>
#include "DependencyManager.h"
#include "ResourceManager.h"
#include "AssetClient.h"
#include "LimitedNodeList.h"
#include "NodeList.h"

#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QByteArray>
#include <QDebug>
#include <QDirIterator>

QTEST_MAIN(VRMTests)

void VRMTests::initTestCase() {
    qRegisterMetaType<QNetworkReply*>("QNetworkReply*");

    DependencyManager::registerInheritance<LimitedNodeList, NodeList>();
    DependencyManager::set<NodeList>(NodeType::Agent, INVALID_PORT);

    DependencyManager::set<ModelFormatRegistry>(); // ModelFormatRegistry must be defined before ModelCache. See the ModelCache constructor.
    DependencyManager::set<ResourceManager>();
    DependencyManager::set<AssetClient>();


    auto modelFormatRegistry = DependencyManager::get<ModelFormatRegistry>();
    modelFormatRegistry->addFormat(FBXSerializer());
    modelFormatRegistry->addFormat(OBJSerializer());
    modelFormatRegistry->addFormat(GLTFSerializer());
}

void VRMTests::loadGLTF_data() {

    QTest::addColumn<QString>("filename");
    QTest::addColumn<bool>("expectParseFail");
    QTest::addColumn<bool>("expectWarnings");
    QTest::addColumn<bool>("expectErrors");

    QTest::newRow("seed-san")                  << "models/src/Seed-san.vrm"                         << false << false << false;
    QTest::newRow("constraint_twist_sample")   << "models/src/VRM1_Constraint_Twist_Sample.vrm"     << false << false << false;

}

void VRMTests::loadGLTF() {
    QFETCH(QString, filename);
    QFETCH(bool, expectParseFail);
    QFETCH(bool, expectWarnings);
    QFETCH(bool, expectErrors);


    QFile gltf_file(filename);
    QVERIFY(gltf_file.open(QIODevice::ReadOnly));

    QByteArray data = gltf_file.readAll();
    QByteArray uncompressedData;
    QUrl url("https://example.com");

    qInfo() << "URL: " << url;

    url.setPath("/" + filename);
    uncompressedData = data;

    ModelLoader loader;
    QMultiHash<QString, QVariant> serializerMapping;
    std::string webMediaType;

    serializerMapping.insert("combineParts", true);
    serializerMapping.insert("deduplicateIndices", true);

    qInfo() << "Loading model from" << uncompressedData.length() << "bytes data, url" << url;

    // Check that we can find a serializer for this
    auto serializer = DependencyManager::get<ModelFormatRegistry>()->getSerializerForMediaType(uncompressedData, url, webMediaType);
    QVERIFY(serializer);



    hfm::Model::Pointer model = loader.load(uncompressedData, serializerMapping, url, webMediaType);
    QVERIFY(expectParseFail == !model);

    if (!model) {
        // We expected this parse to fail, so nothing more to do here.
        return;
    }

    QVERIFY(!model->meshes.empty());
    QVERIFY(!model->joints.empty());

    qInfo() << "Model was loaded with" << model->meshes.count() << "meshes and" << model->joints.count() << "joints. Found" << model->loadWarningCount << "warnings and" << model->loadErrorCount << "errors";

    // Some models we test are expected to be broken. We're testing that we can load the model without blowing up,
    // so loading it with errors is still a successful test.
    QVERIFY(expectWarnings == (model->loadWarningCount>0));
    QVERIFY(expectErrors == (model->loadErrorCount>0));
}
