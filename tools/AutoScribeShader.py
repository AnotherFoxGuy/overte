#!/usr/bin/env python3
# AutoScribeShader.py
#
# Python conversion of AutoScribeShader.cmake
# Original copyright:
# Created by Sam Gateau on 12/17/14.
# Copyright 2014 High Fidelity, Inc.
#
# Distributed under the Apache License, Version 2.0.
# See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

import os
import re
import sys
import argparse
import subprocess
from pathlib import Path
from glob import glob
import json


class ShaderProcessor:
    def __init__(self):
        self.shader_qrc = ""
        self.shader_enums = ""
        self.vertex_enums = ""
        self.fragment_enums = ""
        self.program_enums = ""
        self.shader_programs_array = ""
        self.shader_shaders_array = ""
        self.shader_count = 0
        self.scribed_shaders = []
        self.spirv_shaders = []
        self.reflected_shaders = []
        self.all_shader_headers = []
        self.all_scribe_shaders = []
        self.autoscribe_shader_seen_libs = []
        self.autoscribe_shadergen_commands = ""
        self.hifi_libraries_shader_include_files = []

        # Set defaults - these would normally be set via command line arguments
        self.shaders_dir = os.path.join(os.getcwd(), "shaders")
        self.autoscribe_header_dir = os.path.join(os.getcwd(), "headers")
        self.cmake_source_dir = os.getcwd()
        self.use_gles = False
        self.is_apple = False

        # Ensure the output directory exists
        os.makedirs(self.shaders_dir, exist_ok=True)

    def autoscribe_append_qrc(self, alias, filepath):
        """Append a file entry to the QRC resource file content"""
        self.shader_qrc += f'<file alias="{alias}">{filepath}</file>\n'

    def autoscribe_platform_shader(self, platform_path):
        """Process a shader for a specific platform"""
        # Extract platform information from path
        match = re.search(r"([0-9]+(es)?)(/stereo)?", platform_path)
        if not match:
            return

        autoscribe_dialect = match.group(1)
        if match.group(3):
            autoscribe_variant = "stereo"
        else:
            autoscribe_variant = "mono"

        source_group_path = platform_path.replace("/", "\\")
        source_group_path = f"{self.shader_lib}\\{source_group_path}"

        autoscribe_dialect_header = f"{self.autoscribe_header_dir}/{autoscribe_dialect}/header.glsl"
        autoscribe_variant_header = f"{self.autoscribe_header_dir}/{autoscribe_variant}.glsl"

        # Define output files
        autoscribe_output_file = f"{self.shaders_dir}/{self.shader_lib}/{platform_path}/{self.shader_name}.{self.shader_type}"
        os.makedirs(os.path.dirname(autoscribe_output_file), exist_ok=True)

        self.autoscribe_append_qrc(f"{self.shader_count}/{platform_path}/scribe", autoscribe_output_file)
        self.scribed_shaders.append(autoscribe_output_file)

        # SPIRV file
        autoscribe_spirv_file = f"{autoscribe_output_file}.spv"
        # Don't add unoptimized spirv to the QRC, as in original
        self.spirv_shaders.append(autoscribe_spirv_file)

        # Optimized SPIRV file
        autoscribe_spirv_opt_file = f"{autoscribe_output_file}.opt.spv"
        self.autoscribe_append_qrc(f"{self.shader_count}/{platform_path}/spirv", autoscribe_spirv_opt_file)
        self.spirv_shaders.append(autoscribe_spirv_opt_file)

        # GLSL file from SPIRV
        autoscribe_spirv_glsl_file = f"{autoscribe_output_file}.glsl"
        self.autoscribe_append_qrc(f"{self.shader_count}/{platform_path}/glsl", autoscribe_spirv_glsl_file)
        self.spirv_shaders.append(autoscribe_spirv_glsl_file)

        # JSON file from SPIRV
        autoscribe_spirv_json_file = f"{autoscribe_output_file}.json"
        self.autoscribe_append_qrc(f"{self.shader_count}/{platform_path}/json", autoscribe_spirv_json_file)
        self.reflected_shaders.append(autoscribe_spirv_json_file)

        # Create shader generation command line
        shader_gen_line = [autoscribe_dialect, autoscribe_variant]
        temp_path = os.path.relpath(self.shader_file, self.cmake_source_dir)
        shader_gen_line.append(temp_path)
        temp_path = os.path.relpath(autoscribe_output_file, self.cmake_source_dir)
        shader_gen_line.append(temp_path)

        if self.defines:
            shader_gen_line.append(f"defines:{self.defines}")

        shader_gen_line.extend(self.autoscribe_shader_seen_libs)
        self.autoscribe_shadergen_commands += " ".join(shader_gen_line) + "\n"

    def autoscribe_shader(self, shader_include_files=None):
        """Process a shader file"""

        if shader_include_files is None:
            shader_include_files = []

        # Set the include paths
        shader_includes_paths = []
        for include_file in shader_include_files:
            include_dir = os.path.dirname(include_file)
            if include_dir not in shader_includes_paths:
                shader_includes_paths.append(include_dir)

        for extra_shader_include in self.hifi_libraries_shader_include_files:
            if extra_shader_include not in shader_includes_paths:
                shader_includes_paths.append(extra_shader_include)

        scribe_includes = []
        for include_path in shader_includes_paths:
            scribe_includes.extend(["-I", f"{include_path}/"])

        # Figure out output names
        self.shader_name = os.path.splitext(os.path.basename(self.shader_file))[0]
        shader_ext = os.path.splitext(self.shader_file)[1]

        if shader_ext == ".slv":
            self.shader_type = "vert"
        elif shader_ext == ".slf":
            self.shader_type = "frag"
        elif shader_ext == ".slg":
            self.shader_type = "geom"

        if self.defines:
            self.shader_name = f"{self.shader_name}_{self.defines}"

        scribe_args = ["-D", "GLPROFILE", "GLPROFILE", "-T", self.shader_type] + scribe_includes

        # Write shader name file
        shader_scribed = f"{self.shaders_dir}/{self.shader_lib}/{self.shader_name}.{self.shader_type}"
        shader_name_file = f"{shader_scribed}.name"
        os.makedirs(os.path.dirname(shader_scribed), exist_ok=True)

        with open(shader_name_file, 'w') as f:
            f.write(f"{self.shader_name}.{self.shader_type}")

        self.autoscribe_append_qrc(f"{self.shader_count}/name", shader_name_file)

        # Process for different platforms
        if self.use_gles:
            self.spirv_cross_args = ["--version", "310es"]
            self.autoscribe_platform_shader("310es")
            self.autoscribe_platform_shader("310es/stereo")
        else:
            self.spirv_cross_args = ["--version", "410", "--no-420pack-extension"]
            self.autoscribe_platform_shader("410")
            self.autoscribe_platform_shader("410/stereo")
            if not self.is_apple:
                self.spirv_cross_args = ["--version", "450"]
                self.autoscribe_platform_shader("450")
                self.autoscribe_platform_shader("450/stereo")

        self.shader_count += 1
        self.shader_shaders_array += f"{self.shader_count},\n"
        # print(f"{self.shader_name} = {self.shader_count}")
        return f"{self.shader_name} = {self.shader_count},\n"

    # This function takes in the list of defines, which would look like:
    # (normalmap translucent:f)/shadow deformed:v
    # and handles parentheses and slashes, producing the final list of all combinations, which in that case will look like:
    # normalmap translucent:f normalmap_translucent:f shadow normalmap_deformed:v translucent:f_deformed:v normalmap_translucent:f_deformed:v shadow_deformed:v
    # translucent:f forward:f
    # translucent:f forward:f translucent:f_forward:f

    # translucent:f/forward:f
    # translucent:f forward:f

    def generate_defines(self, inp):
        """Helper function to generate shader define combinations"""
        return_list = inp.split(' ')



        for df in return_list:
            if '/' in df:
                return_list.remove(df)
                return_list += df.split("/")

        return return_list


    #     """Helper function to generate shader define combinations"""
    #     if not input_list:
    #         return []
    #
    #     # This while loop handles parentheses, looking for matching ( and ) and then calling GENERATE_DEFINES_LIST_HELPER recursively on the text in between
    #     str_length = len(input_list)
    #     open_index = -1
    #     str_index = 0
    #     nested_depth = 0
    #
    #     while str_index < str_length:
    #         current_char = input_list[str_index]
    #
    #         if current_char == "(" and open_index == -1:
    #             open_index = str_index
    #             str_index += 1
    #             continue
    #         elif current_char == "(" and open_index != -1:
    #             nested_depth += 1
    #             str_index += 1
    #             continue
    #         elif current_char == ")" and open_index != -1 and nested_depth > 0:
    #             nested_depth -= 1
    #             str_index += 1
    #             continue
    #         elif current_char == ")" and open_index != -1 and nested_depth == 0:
    #             group_str = input_list[open_index + 1:str_index]
    #             expanded_group_list = self.generate_defines(group_str)
    #             expanded_group_str = "/".join(expanded_group_list)
    #             input_list = input_list.replace(f"({group_str})", expanded_group_str)
    #             str_index = open_index - 1
    #             open_index = -1
    #             str_length = len(input_list)
    #             continue
    #
    #         str_index += 1
    #
    #     # Here we handle the base case, the recursive case, and slashes
    #     input_list = re.split(r'[/,\s]+', input_list)
    #
    #     return self.generate_defines_list(input_list)
    #
    # def generate_defines_list(self, input_list):
    #     # Handle the recursive case
    #
    #     print(input_list)
    #     if len(input_list) == 1:
    #         if isinstance(input_list[0], str):
    #             return input_list[0].split("/")
    #         return input_list
    #     elif len(input_list) > 1:
    #         current_defines = input_list[0].split("/") if isinstance(input_list[0], str) else input_list[0]
    #         remaining_defines_list = self.generate_defines_list(input_list[1:])
    #
    #         to_return_list = current_defines.copy()
    #         for remaining_define in remaining_defines_list:
    #             to_return_list.append(remaining_define)
    #             for current_define in current_defines:
    #                 to_return_list.append(f"{current_define}_{remaining_define}")
    #
    #         return to_return_list
    #
    #     return []

    def autoscribe_shader_lib(self, shader_lib):
        """Process a shader library"""
        self.shader_lib = shader_lib
        os.makedirs(f"{self.shaders_dir}/{self.shader_lib}", exist_ok=True)

        self.shader_count = 0

        self.hifi_libraries_shader_include_files.append(f"{self.cmake_source_dir}/libraries/{shader_lib}/src")
        shader_namespace = shader_lib.replace("-", "_")
        self.shader_enums += f"namespace {shader_namespace} {{\n"

        src_folder = f"{self.cmake_source_dir}/libraries/{shader_lib}/src"

        # Process shader header files
        shader_include_files = glob(os.path.join(src_folder, '*', '*.slh'), recursive=True)
        shader_include_files.sort()

        if shader_include_files:
            self.all_shader_headers.extend(shader_include_files)
            self.all_scribe_shaders.extend(shader_include_files)

        # Process vertex shader files
        shader_vertex_files = glob(os.path.join(src_folder, '*', '*.slv'), recursive=True)
        shader_vertex_files.sort()

        if shader_vertex_files:
            self.all_scribe_shaders.extend(shader_vertex_files)

            self.vertex_enums = "namespace vertex { enum {\n"

            for shader_file in shader_vertex_files:
                self.shader_file = shader_file
                self.defines = ""
                self.vertex_enums += self.autoscribe_shader(self.all_shader_headers)

        # Process fragment shader files
        shader_fragment_files = glob(os.path.join(src_folder, '*', '*.slf'), recursive=True)
        shader_fragment_files.sort()

        if shader_fragment_files:
            self.all_scribe_shaders.extend(shader_fragment_files)

            self.fragment_enums = "namespace fragment { enum {\n"
            for shader_file in shader_fragment_files:
                self.shader_file = shader_file
                self.defines = ""
                self.fragment_enums += self.autoscribe_shader(self.all_shader_headers)

        # Process program files
        shader_program_files = glob(os.path.join(src_folder, '*', '*.slp'), recursive=True)
        shader_program_files.sort()

        if shader_program_files:
            self.all_scribe_shaders.extend(shader_program_files)
            self.program_enums = "namespace program { enum {\n"

            for program_file in shader_program_files:
                program_name = os.path.splitext(os.path.basename(program_file))[0]
                program_folder = os.path.dirname(program_file)

                with open(program_file, 'r') as f:
                    program_config = f.read()

                autoscribe_program_vertex = program_name
                autoscribe_program_fragment = program_name
                autoscribe_program_defines = ""

                if program_config:
                    vertex_match = re.search(r".*VERTEX\s+([_\\:A-Z0-9a-z]+)", program_config)
                    if vertex_match:
                        autoscribe_program_vertex = vertex_match.group(1)

                    frag_match = re.search(r".*FRAGMENT\s+([_:A-Z0-9a-z]+)", program_config)
                    if frag_match:
                        autoscribe_program_fragment = frag_match.group(1)

                    def_match = re.search(r".*DEFINES\s+([a-zA-Z\(\)/: ]+)", program_config)
                    if def_match:
                        autoscribe_program_defines = def_match.group(1).lower().split(' ')

                if not re.search(r".*::.*", autoscribe_program_vertex):
                    autoscribe_program_vertex = f"vertex::{autoscribe_program_vertex}"

                if not re.search(r".*::.*", autoscribe_program_fragment):
                    autoscribe_program_fragment = f"fragment::{autoscribe_program_fragment}"

                vertex_name = re.sub(r".*::", "", autoscribe_program_vertex)
                fragment_name = re.sub(r".*::", "", autoscribe_program_fragment)

                print(f"{program_name}:")
                print(f"\t {autoscribe_program_defines}")

                defines_list = self.generate_defines_list(autoscribe_program_defines)
                # defines_list = []
                print(f"\t{defines_list}")

                self.program_enums += f"{program_name} = ({autoscribe_program_vertex} << 16) | {autoscribe_program_fragment},\n"
                self.shader_programs_array += f" {shader_namespace}::program::{program_name},\n"

                for defines in defines_list:
                    if not defines:
                        continue

                    orig_defines = defines

                    # Below here we handle :v and :f. The program name includes both, but the vertex and fragment names
                    # remove the elements with :f and :v respectively, and only have to call AUTOSCRIBE_SHADER if they don't have those
                    # (because the shaders without them will have already been generated)
                    vertex_defines = orig_defines.replace(":v", "")
                    has_fragment = ":f" in orig_defines

                    if not has_fragment:
                        self.defines = vertex_defines

                        shader_file = f"{program_folder}/{vertex_name}.slv"
                        if not os.path.exists(shader_file):
                            shader_file = f"{program_folder}/../{vertex_name}.slv"

                        if os.path.exists(shader_file):
                            self.shader_file = shader_file
                            # print(f"AAAAAAAAAAAAAA {shader_file}")
                            self.vertex_enums += self.autoscribe_shader(self.all_shader_headers)
                    else:
                        vertex_defines = re.sub(r"_*[^_]*:f", "", vertex_defines)

                    if vertex_defines and not vertex_defines.startswith("_"):
                        vertex_defines = f"_{vertex_defines}"

                    fragment_defines = orig_defines.replace(":f", "")
                    has_vertex = ":v" in orig_defines

                    if not has_vertex:
                        self.defines = fragment_defines

                        shader_file = f"{program_folder}/{fragment_name}.slf"
                        if not os.path.exists(shader_file):
                            shader_file = f"{program_folder}/../{fragment_name}.slf"

                        if os.path.exists(shader_file):
                            self.shader_file = shader_file
                            self.fragment_enums += self.autoscribe_shader(self.all_shader_headers)
                    else:
                        fragment_defines = re.sub(r"_*[^_]*:v", "", fragment_defines)

                    if fragment_defines and not fragment_defines.startswith("_"):
                        fragment_defines = f"_{fragment_defines}"

                    program_defines = re.sub(r":(f|v)", "", orig_defines)

                    if program_defines:
                        self.program_enums += f"{program_name}_{program_defines} = ({autoscribe_program_vertex}{vertex_defines} << 16) | {autoscribe_program_fragment}{fragment_defines},\n"
                        self.shader_programs_array += f" {shader_namespace}::program::{program_name}_{program_defines},\n"

        # Finish the shader enums
        if shader_vertex_files:
            self.vertex_enums += "}; } // vertex \n"
            self.shader_enums += self.vertex_enums

        if shader_fragment_files:
            self.fragment_enums += "}; } // fragment \n"
            self.shader_enums += self.fragment_enums

        if shader_program_files:
            self.program_enums += "}; } // program \n"
            self.shader_enums += self.program_enums

        self.shader_enums += f"}} // namespace {shader_namespace}\n"

    def autoscribe_shader_libs(self, shader_libs):
        """Process multiple shader libraries"""
        print("Shader processing start")

        for shader_lib in shader_libs:
            self.autoscribe_shader_seen_libs.append(shader_lib)
            self.autoscribe_shader_lib(shader_lib)

        # Generate the library files
        self.write_shader_enums_cpp()
        self.write_shader_enums_h()
        self.write_shaders_qrc()

        # Write the shadergen command list
        with open(os.path.join(self.shaders_dir, "shadergen.txt"), 'w') as f:
            f.write(self.autoscribe_shadergen_commands)

        print("Shader processing end")

    def write_shader_enums_cpp(self):
        content = f"""// This file was auto-generated by AutoScribeShader.py
#include "ShaderEnums.h"
#include <vector>

namespace shader {{
    const std::vector<uint32_t>& allPrograms() {{
        static const std::vector<uint32_t> ALL_PROGRAMS{{{{
            {self.shader_programs_array}
        }}}};
        return ALL_PROGRAMS;
    }}

    const std::vector<uint32_t>& allShaders() {{
        static const std::vector<uint32_t> ALL_SHADERS{{{{
            {self.shader_shaders_array}
        }}}};
        return ALL_SHADERS;
    }}
}}
"""
        with open(os.path.join(self.shaders_dir, "ShaderEnums.cpp"), 'w') as f:
            f.write(content)

    def write_shader_enums_h(self):
        """Write ShaderEnums.h file"""
        content = f"""// This file was auto-generated by AutoScribeShader.py
#include <cstdint>
#include <string>

namespace shader {{
    {self.shader_enums}
}}
"""
        with open(os.path.join(self.shaders_dir, "ShaderEnums.h"), 'w') as f:
            f.write(content)

    def write_shaders_qrc(self):
        """Write shaders.qrc file"""
        content = f"""<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/shaders">
        {self.shader_qrc}
    </qresource>
</RCC>
"""
        with open(os.path.join(self.shaders_dir, "shaders.qrc"), 'w') as f:
            f.write(content)


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Shader processing script')
    parser.add_argument('--source-dir', help='Source directory', default=os.getcwd())
    parser.add_argument('--shader-libs', nargs='+', help='Shader libraries to process')
    parser.add_argument('--output-dir', help='Output directory', default=os.path.join(os.getcwd(), "shaders"))
    parser.add_argument('--use-gles', action='store_true', help='Use GLES')
    parser.add_argument('--is-apple', action='store_true', help='Is running on Apple platform')
    args = parser.parse_args()

    processor = ShaderProcessor()
    processor.cmake_source_dir = args.source_dir
    processor.shaders_dir = args.output_dir
    processor.use_gles = args.use_gles
    processor.is_apple = args.is_apple

    if args.shader_libs:
        processor.autoscribe_shader_libs(args.shader_libs)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
