#!/usr/bin/env python3
import unittest

from lua_binding_call_shapes import arg_kind, iter_calls, split_args


class LuaBindingCallShapesTests(unittest.TestCase):
    def test_split_args_respects_nested_calls_and_strings(self):
        body = "'a,b', nested(1, 2), {x = 1, y = 2}, value"
        self.assertEqual(
            split_args(body),
            ["'a,b'", "nested(1, 2)", "{x = 1, y = 2}", "value"],
        )

    def test_direct_native_call_shape(self):
        calls = list(iter_calls('Lua_CopyFile("a", path .. "/b")'))
        self.assertEqual(len(calls), 1)
        api, kind, args = calls[0]
        self.assertEqual(api, "Lua_CopyFile")
        self.assertEqual(kind, "global")
        self.assertEqual(len(args), 2)
        self.assertEqual([arg_kind(value) for value in args], ["string", "expression"])

    def test_native_member_call_shape(self):
        calls = list(iter_calls("ProtoRPC:new(1001, callback)"))
        self.assertEqual(calls, [("ProtoRPC:new", "member", ["1001", "callback"])])

    def test_non_native_calls_are_ignored(self):
        self.assertEqual(list(iter_calls("print('x'); manager:open(1)")), [])


if __name__ == "__main__":
    unittest.main()
