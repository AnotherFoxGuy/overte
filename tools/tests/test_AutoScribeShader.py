import unittest

from tools.AutoScribeShader import ShaderProcessor


class AutoScribeShaderTests(unittest.TestCase):

    def test_generate_defines_list_basic(self):
        processor = ShaderProcessor()
        # inp = ["translucent:f","forward:f"]
        inp = "translucent:f/forward:f"
        out = processor.generate_defines(inp)
        print("------------------------------------------")
        print(out)

        self.assertListEqual(out, ["translucent:f", "forward:f"])

    def test_generate_defines_list_simple(self):
        processor = ShaderProcessor()
        # inp = ["translucent:f","forward:f"]
        inp = "translucent:f forward:f"
        out = processor.generate_defines(inp)
        print("------------------------------------------")
        print(out)

        self.assertListEqual(out, ["translucent:f", "forward:f", "translucent:f_forward:f"])

    def test_generate_defines_list_parm(self):
        processor = ShaderProcessor()
        inp = "(translucent unlit:f)/forward mirror:f"
        out = processor.generate_defines(inp)
        print("------------------------------------------")
        print(out)

        self.assertListEqual(out, ["translucent", "unlit:f", "translucent_unlit:f", "forward", "mirror:f",
                                   "translucent_mirror:f", "unlit:f_mirror:f", "translucent_unlit:f_mirror:f",
                                   "forward_mirror:f"])


if __name__ == '__main__':
    unittest.main()
