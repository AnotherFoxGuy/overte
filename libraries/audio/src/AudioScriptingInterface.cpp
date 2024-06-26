//
//  AudioScriptingInterface.cpp
//  libraries/audio/src
//
//  Created by Stephen Birarda on 1/2/2014.
//  Copyright 2014 High Fidelity, Inc.
//  Copyright 2022-2023 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//  SPDX-License-Identifier: Apache-2.0
//

#include "AudioScriptingInterface.h"

#include <QVector3D>

#include <shared/QtHelpers.h>

#include "ScriptAudioInjector.h"
#include <ScriptEngine.h>
#include <ScriptEngineCast.h>
#include <ScriptEngineLogging.h>
#include <ScriptManager.h>

STATIC_SCRIPT_TYPES_INITIALIZER(+[](ScriptManager* manager){
    auto scriptEngine = manager->engine().get();

    registerAudioMetaTypes(scriptEngine);
});

STATIC_SCRIPT_INITIALIZER(+[](ScriptManager* manager){
    auto scriptEngine = manager->engine().get();

    scriptEngine->registerGlobalObject("Audio", DependencyManager::get<AudioScriptingInterface>().data());
});

void registerAudioMetaTypes(ScriptEngine* engine) {
    scriptRegisterMetaType<AudioInjectorOptions, injectorOptionsToScriptValue, injectorOptionsFromScriptValue>(engine);
    scriptRegisterMetaType<SharedSoundPointer, soundSharedPointerToScriptValue, soundSharedPointerFromScriptValue>(engine);
}


void AudioScriptingInterface::setLocalAudioInterface(AbstractAudioInterface* audioInterface) {
    if (_localAudioInterface) {
        disconnect(_localAudioInterface, &AbstractAudioInterface::isStereoInputChanged,
                   this, &AudioScriptingInterface::isStereoInputChanged);
    }
    
    _localAudioInterface = audioInterface;

    if (_localAudioInterface) {
        connect(_localAudioInterface, &AbstractAudioInterface::isStereoInputChanged,
                this, &AudioScriptingInterface::isStereoInputChanged);
    }
}

ScriptAudioInjector* AudioScriptingInterface::playSystemSound(SharedSoundPointer sound) {
    AudioInjectorOptions options;
    options.localOnly = true;
    options.positionSet = false;    // system sound
    return playSound(sound, options);
}

ScriptAudioInjector* AudioScriptingInterface::playSound(SharedSoundPointer sound, const AudioInjectorOptions& injectorOptions) {
    if (sound) {
        // stereo option isn't set from script, this comes from sound metadata or filename
        AudioInjectorOptions optionsCopy = injectorOptions;
        optionsCopy.stereo = sound->isStereo();
        optionsCopy.ambisonic = sound->isAmbisonic();
        optionsCopy.localOnly = optionsCopy.localOnly || sound->isAmbisonic();  // force localOnly when Ambisonic

        auto injector = DependencyManager::get<AudioInjectorManager>()->playSound(sound, optionsCopy);
        if (!injector) {
            return nullptr;
        }
        return new ScriptAudioInjector(injector);

    } else {
        qCDebug(scriptengine) << "AudioScriptingInterface::playSound called with null Sound object.";
        return nullptr;
    }
}

void AudioScriptingInterface::setStereoInput(bool stereo) {
    if (_localAudioInterface) {
        QMetaObject::invokeMethod(_localAudioInterface, "setIsStereoInput", Q_ARG(bool, stereo));
    }
}

bool AudioScriptingInterface::isStereoInput() {
    bool stereoEnabled = false;
    if (_localAudioInterface) {
        stereoEnabled = _localAudioInterface->isStereoInput();
    }
    return stereoEnabled;
}

bool AudioScriptingInterface::getServerEcho() {
    bool serverEchoEnabled = false;
    if (_localAudioInterface) {
        serverEchoEnabled = _localAudioInterface->getServerEcho();
    }
    return serverEchoEnabled;
}

void AudioScriptingInterface::setServerEcho(bool serverEcho) {
    if (_localAudioInterface) {
        QMetaObject::invokeMethod(_localAudioInterface, "setServerEcho", Q_ARG(bool, serverEcho));
    }
}

void AudioScriptingInterface::toggleServerEcho() {
    if (_localAudioInterface) {
        QMetaObject::invokeMethod(_localAudioInterface, "toggleServerEcho");
    }
}

bool AudioScriptingInterface::getLocalEcho() {
    bool localEchoEnabled = false;
    if (_localAudioInterface) {
        localEchoEnabled = _localAudioInterface->getLocalEcho();
    }
    return localEchoEnabled;
}

void AudioScriptingInterface::setLocalEcho(bool localEcho) {
    if (_localAudioInterface) {
        QMetaObject::invokeMethod(_localAudioInterface, "setLocalEcho", Q_ARG(bool, localEcho));
    }
}

void AudioScriptingInterface::toggleLocalEcho() {
    if (_localAudioInterface) {
        QMetaObject::invokeMethod(_localAudioInterface, "toggleLocalEcho");
    }
}
