import os
from conan.tools.files import get, copy, download
from conan import ConanFile


class WebRTCConan(ConanFile):
    name = "libwebrtc-bin"
    version = "120.6099.1.1"
    settings = "os", "arch"

    def build_requirements(self):
        if self.settings.os == "Windows": 
            self.tool_requires("7zip/23.01")

    def build(self):
        # https://github.com/crow-misia/libwebrtc-bin/releases/download/120.6099.1.1/libwebrtc-win-x64.7z
        base_url = "https://github.com/crow-misia/libwebrtc-bin/releases/download/"

        _os = {"Windows": "win", "Linux": "linux", "Macos": "macos"}.get(
            str(self.settings.os)
        )
        _arch = "x64" if '64' in self.settings.arch else "x86"
        _ext = "7z" if self.settings.os == "Windows" else "tar.xz"
        url = "{}/{}/libwebrtc-{}-{}.{}".format(
            base_url, self.version, _os, _arch, _ext
        )

        if self.settings.os == "Windows": 
            download(self, url, "dl.7z")
            self.run("7z x dl.7z")
        else: 
            get(self, url)

    def package(self):
        if self.settings.os == "Windows": 
            _folder = "debug" if self.settings.os == "Windows" else "release"
        else: 
            _folder = "lib"
       
        copy(
            self,
            "*.h",
            os.path.join(self.build_folder, "include"),
            os.path.join(self.package_folder, "include"),
        )
        copy(
            self,
            "*.lib",
            os.path.join(self.build_folder, _folder),
            os.path.join(self.package_folder, "lib"),
        )
        copy(
            self,
            "*.a",
            os.path.join(self.build_folder, _folder),
            os.path.join(self.package_folder, "lib"),
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "WebRTC")
        self.cpp_info.set_property("cmake_target_name", "WebRTC::WebRTC")
        self.cpp_info.libs = ["webrtc"]
