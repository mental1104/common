#!/usr/bin/env python3
import sys
import argparse
from mental1104.connector.pulsar import PulsarAdminHelper

def validate_and_process_arg(arg):
    # 检查参数中 `/` 的数量
    if arg.count("/") > 2:
        print("错误: 参数中最多只能包含两个 '/'，请重新输入。")
        sys.exit(1)

    # 根据 `/` 分割参数
    split_result = arg.split("/")

    # 根据分割结果生成元组
    while len(split_result) < 3:
        split_result.append(None)  # 如果不足三个部分，补充 `None`

    # 拆包为三个变量
    part1, part2, part3 = split_result

    # 返回拆分后的结果
    return part1, part2, part3


if __name__ == "__main__":
    # 定义命令行参数解析器
    parser = argparse.ArgumentParser(description="创建 Pulsar 主题。")
    parser.add_argument(
        "topic_path",
        help="主题路径，例如：tenant/namespace/topic 或 tenant/namespace。",
    )
    parser.add_argument(
        "--partitions",
        type=int,
        default=0,
        help="指定分区数。如果大于 0，则创建分区主题。",
    )

    # 解析命令行参数
    args = parser.parse_args()
    tenant, namespace, topic = validate_and_process_arg(args.topic_path)

    # 调用 ensure_tenant_namespace_topic 并传入分区数
    PulsarAdminHelper.ensure_tenant_namespace_topic(tenant, namespace, topic, args.partitions)

    print(f"{'分区主题' if args.partitions > 0 else '非分区主题'} 已创建: {tenant}/{namespace}/{topic if topic else ''}")
