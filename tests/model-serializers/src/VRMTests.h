//
//  VRMTests.h
//  tests/model-serializers/src
//
//  Created by Dale Glass on 20/11/2022.
//  Copyright 2022 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef overte_VRMTests_h
#define overte_VRMTests_h

#include <QtTest/QtTest>

class VRMTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadGLTF_data();
    void loadGLTF();

};

#endif // overte_VRMTests_h
