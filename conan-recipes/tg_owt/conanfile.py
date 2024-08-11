import os
from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.files import get, collect_libs
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout


class tgowtConan(ConanFile):
    name = "tg_owt"
    version = "2024.08"
    _commit = "9c6af3c10101a355cb42cd36046514381449c464"
    license = "MIT"
    url = "https://github.com/desktop-app/tg_owt"
    description = "WebRTC build for Telegram"
    settings = "os", "compiler", "build_type", "arch"
    implements = ["auto_shared_fpic"]
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def layout(self):
        cmake_layout(self)

    def requirements(self):
    #     self.requires("crc32c/1.1.2")
    #     self.requires("libsrtp/2.6.0")
        self.requires("abseil/20240116.2")
    #     # self.requires("libyuv/cci.20201106")

    def source(self):
        git = Git(self)
        git.clone(
            url="https://github.com/AnotherFoxGuy/tg_owt.git",
            target=".",
            hide_url=False,
        )
        git.checkout(self._commit)
        git.run("submodule update --init --recursive")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["TG_OWT_USE_X11"] = "OFF"
        tc.variables["TG_OWT_USE_PIPEWIRE"] = "OFF"
        tc.variables["TG_OWT_DLOPEN_PIPEWIRE"] = "OFF"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = collect_libs(self)
