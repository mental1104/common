import pytest

from mental1104 import dispatch_for


def op(*args):
    """Overloaded operation used in tests."""
    raise NotImplementedError("Implementation is provided by OpImpl.")


@dispatch_for(op)
class OpImpl:
    # (int, int) overload
    @dispatch_for(int, int)
    def op_int_int(self, x: int, y: int):
        return x + y

    # (str, str) overload
    @dispatch_for(str, str)
    def op_str_str(self, x: str, y: str):
        return f"{x}-{y}"

    # (bytes, str) overload: decode and redirect to (str, str)
    @dispatch_for(bytes, str)
    def op_bytes_str(self, x: bytes, y: str):
        text = x.decode("utf-8")
        # redirect: call the same entry with new arguments
        return self(text, y)

    # Example with 3 args: (int, int, int)
    @dispatch_for(int, int, int)
    def op_int_int_int(self, a: int, b: int, c: int):
        return a + b + c

    # Fallback if nothing matches
    def default(self, *args, **kwargs):
        types = tuple(type(a) for a in args)
        raise TypeError(f"No overload for op with types {types}")


class TestDispatchFor:
    def test_overload_matches_registered_patterns(self):
        """
        【场景背景】按示例注册多重重载后, dispatch_for 应根据入参类型路由到对应实现。
        【步骤输入】分别传入 (1,2)、("foo","bar")、(b"foo","bar")、(1,2,3)。
        【期望输出】返回 3、"foo-bar"、"foo-bar"、6, 证明解码与转发也生效。
        """
        assert op(1, 2) == 3
        assert op("foo", "bar") == "foo-bar"
        assert op(b"foo", "bar") == "foo-bar"
        assert op(1, 2, 3) == 6

    def test_default_called_when_no_match(self):
        """
        【场景背景】当参数类型或数量没有注册时, 应落入 default 并抛出 TypeError。
        【步骤输入】调用 op(1, "bar") 以及 op(42)。
        【期望输出】两次都抛 TypeError 且消息包含 "No overload for op with types"。
        """
        with pytest.raises(TypeError, match="No overload for op with types"):
            op(1, "bar")
        with pytest.raises(TypeError, match="No overload for op with types"):
            op(42)

    def test_instance_call_reuses_dispatcher(self):
        """
        【场景背景】类实例应与入口函数共享同一个分发器。
        【步骤输入】实例化 OpImpl 后调用 impl(2,3) 与 impl(b"x", "y")。
        【期望输出】得到 5 与 "x-y", 证明 __call__ 复用相同路由。
        """
        impl = OpImpl()
        assert impl(2, 3) == 5
        assert impl(b"x", "y") == "x-y"

    def test_duplicate_overload_pattern_raises(self):
        """
        【场景背景】同一个模式重复注册应被明确拒绝。
        【步骤输入】为同一入口函数的两个方法都标注 @dispatch_for(int)。
        【期望输出】装饰类时抛 TypeError, 提示 Duplicate overload。
        """

        def duplicated(*args):
            raise NotImplementedError

        with pytest.raises(TypeError, match="Duplicate overload"):

            @dispatch_for(duplicated)
            class DuplicateImpl:
                @dispatch_for(int)
                def handle_int(self, value: int):
                    return value

                @dispatch_for(int)
                def handle_int_again(self, value: int):
                    return value
