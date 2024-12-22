import json
from dataclasses import asdict

class JsonSerializable:
    def __str__(self):
        # JSON 格式化输出，面向用户
        return json.dumps(self.to_dict(), indent=4)

    def __repr__(self):
        # 紧凑的 JSON 输出，面向开发者
        return json.dumps(self.to_dict())

    def to_dict(self):
        # 将类的属性转换为字典
        # 默认尝试 dataclass 的 asdict 方法
        if hasattr(self, "__dataclass_fields__"):  # 判断是否为 dataclass
            return asdict(self)
        else:
            # 对于普通类，动态提取 __dict__
            return {k: v for k, v in self.__dict__.items() if not k.startswith("_")}