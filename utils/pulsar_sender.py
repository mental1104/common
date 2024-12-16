import argparse
import json
import os
import sys
from enum import Enum
from pulsar import Client, AuthenticationToken
from pulsar.schema import AvroSchema


class EnvironmentVariables(Enum):
    PULSAR_URL = "PULSAR_URL"
    PULSAR_PRODUCER_PORT = "PULSAR_PRODUCER_PORT"
    PULSAR_TOKEN = "PULSAR_TOKEN"
    PULSAR_SCHEMA_PATH = "PULSAR_SCHEMA_PATH"
    PULSAR_DATA_PATH = "PULSAR_DATA_PATH"


class Environment:
    @staticmethod
    def check_required_env_vars(required_env_vars):
        """
        检查是否具有给定的环境变量，若没有，则中止执行并打印缺少的环境变量。

        :param required_env_vars: 需要检查的环境变量列表
        """
        missing_vars = [var.value for var in required_env_vars if var.value not in os.environ]
        if missing_vars:
            print(f"Error: Missing required environment variables: {', '.join(missing_vars)}")
            sys.exit(1)  # 中止执行并返回非零状态


class PulsarMessageSender:
    def __init__(self, pulsar_url: str, pulsar_port: int, full_topic: str, schema_path: str, token: str = None):
        """
        初始化 Pulsar 客户端和生产者。

        :param pulsar_url: Pulsar 服务的 URL，不包含协议或端口，例如：localhost。
        :param pulsar_port: Pulsar 服务的生产者端口（通常为 6650）。
        :param full_topic: 完整的 Topic 名称（格式：tenant/namespace/topic）。
        :param schema_path: Schema 文件路径。
        :param token: Pulsar 身份验证 Token（可选）。
        """
        self.full_topic = full_topic
        self.schema = self._load_schema(schema_path)

        # 构造完整的 pulsar:// 服务地址
        service_url = f"pulsar://{pulsar_url}:{pulsar_port}"

        # 配置 Pulsar 客户端
        if token:
            auth = AuthenticationToken(token)
            self.client = Client(service_url, authentication=auth)
        else:
            self.client = Client(service_url)

        self.producer = self.client.create_producer(
            topic=self.full_topic,
            schema=self.schema,
            block_if_queue_full=True,
            batching_enabled=True
        )

    def _load_schema(self, schema_path: str):
        """
        加载 Schema 文件并返回 AvroSchema。

        :param schema_path: Schema 文件路径，可能是目录或文件。
        :return: AvroSchema 对象。
        """
        # 如果 schema_path 是 None 或空，则从环境变量 PULSAR_SCHEMA_PATH 获取
        if not schema_path:
            schema_path = os.getenv(EnvironmentVariables.PULSAR_SCHEMA_PATH.value)

        if not schema_path:
            print("Error: No schema path provided and PULSAR_SCHEMA_PATH environment variable is not set.")
            sys.exit(1)

        if os.path.isdir(schema_path):
            # 如果是目录，查找与 topic 名称相同的 JSON 文件
            schema_file = os.path.join(schema_path, f"{self.full_topic.split('/')[-1]}.json")
            if not os.path.exists(schema_file):
                print(f"Error: Schema file {schema_file} not found in the directory.")
                sys.exit(1)  # 如果目录下没有找到该文件，退出
            schema_path = schema_file  # 更新 schema_path 为找到的文件路径
    
        # 如果不是目录且不是文件，输出错误
        if not os.path.isfile(schema_path):
            print(f"Error: Schema path {schema_path} is not a valid file.")
            sys.exit(1)

        # 读取 schema 文件
        with open(schema_path, 'r') as file:
            schema_dict = json.load(file)

        # 使用 Pulsar 的 AvroSchema 来处理 JSON schema
        return AvroSchema(None, schema_dict)

    def send_message(self, message_data: dict, data_file: str):
        """
        向指定 Topic 发送消息。

        :param message_data: 消息数据字典。
        :param data_file: 当前正在发送的文件路径，用于异常日志输出。
        """
        try:
            self.producer.send(message_data)
            print(f"Message sent successfully: {json.dumps(message_data, ensure_ascii=False)}")
        except Exception as e:
            print(f"Error: Failed to send message from file {data_file}. Exception: {e}")
            raise  # 重新抛出异常

    def close(self):
        """关闭生产者和客户端。"""
        self.producer.close()
        self.client.close()


def parse_arguments():
    """
    解析命令行参数。

    :return: 参数对象。
    """
    parser = argparse.ArgumentParser(description="向 Pulsar 主题发送消息。")
    # Pulsar 服务地址
    parser.add_argument('--pulsar-url', type=str, help=f"Pulsar 服务主机地址（不包含协议和端口，例如：localhost）。可以通过环境变量 {EnvironmentVariables.PULSAR_URL.value} 来设置。")
    # Pulsar 服务端口
    parser.add_argument('--pulsar-port', type=int, help=f"Pulsar 服务生产者端口（例如：6650）。可以通过环境变量 {EnvironmentVariables.PULSAR_PRODUCER_PORT.value} 来设置。")
    # 完整主题名称
    parser.add_argument('--full-topic', type=str, required=True, help="完整的主题名称（例如：tenant/namespace/topic）。这是必需的，无法通过环境变量替代。")
    # Schema 文件路径
    parser.add_argument(
        '--schema-path',
        type=str,
        help=f"Schema 文件或包含 Schema 的目录路径。可以通过环境变量 {EnvironmentVariables.PULSAR_SCHEMA_PATH.value} 来设置。若该字段是个目录，则会寻找该目录下与topic同名的json文件作为schema。"
    )
    # 数据文件路径
    parser.add_argument('--data-path', type=str, help=f"包含消息数据的 JSON 文件或目录路径。可以通过环境变量 {EnvironmentVariables.PULSAR_DATA_PATH.value} 来设置。")
    # Pulsar 认证 Token
    parser.add_argument('--token', type=str, help=f"Pulsar 认证 Token（可选）。可以通过环境变量 {EnvironmentVariables.PULSAR_TOKEN.value} 来设置。")
    return parser.parse_args()



def get_pulsar_service(cli_url, cli_producer_port):
    """
    获取 Pulsar 服务的生产者端口和主机地址。

    :param cli_url: 从命令行传入的 URL（主机地址，不含协议）。
    :param cli_producer_port: 从命令行传入的生产者端口。
    :return: 主机地址和生产者端口。
    """
    pulsar_url = cli_url or os.getenv(EnvironmentVariables.PULSAR_URL.value)
    producer_port = cli_producer_port or os.getenv(EnvironmentVariables.PULSAR_PRODUCER_PORT.value)

    if not pulsar_url or not producer_port:
        print("Error: Missing Pulsar URL or producer port. Provide them via command-line arguments or environment variables.")
        sys.exit(1)

    return pulsar_url, int(producer_port)


def get_pulsar_token(cli_token):
    """
    获取 Pulsar 认证 Token。

    :param cli_token: 从命令行传入的 Token。
    :return: Pulsar Token。
    """
    return cli_token or os.getenv(EnvironmentVariables.PULSAR_TOKEN.value)


def get_data_paths(cli_data_path):
    """
    获取消息数据文件路径，优先从命令行获取，如果没有，则从环境变量获取。

    :param cli_data_path: 从命令行传入的文件路径。
    :return: 数据文件路径，可能是目录或文件。
    """
    return cli_data_path or os.getenv(EnvironmentVariables.PULSAR_DATA_PATH.value)


def process_data_path(data_path):
    """
    处理数据路径，如果是目录则遍历目录下的所有文件，如果是文件直接返回。
    :param data_path: 文件或目录路径。
    :return: 文件路径列表。
    """
    if os.path.isdir(data_path):
        # 如果是目录，遍历该目录下的所有 JSON 文件
        files = [os.path.join(data_path, f) for f in os.listdir(data_path) if f.endswith('.json')]
        if not files:
            print(f"Error: No JSON files found in directory {data_path}.")
            sys.exit(1)
        return files
    elif os.path.isfile(data_path) and data_path.endswith('.json'):
        return [data_path]  # 如果是文件，返回包含该文件的列表
    else:
        print(f"Error: {data_path} is not a valid file or directory.")
        sys.exit(1)


def main():
    args = parse_arguments()

    # 获取 Pulsar 服务地址和端口
    pulsar_url, producer_port = get_pulsar_service(
        cli_url=args.pulsar_url,
        cli_producer_port=args.pulsar_port
    )

    # 获取 Pulsar Token
    token = get_pulsar_token(args.token)

    # 获取数据路径（如果未提供命令行参数，则从环境变量获取）
    data_path = get_data_paths(args.data_path)

    # 打印调试信息
    print(f"Connecting to Pulsar broker at: pulsar://{pulsar_url}:{producer_port}")

    if not data_path:
        print("Error: No data path provided and PULSAR_DATA_PATH environment variable is not set.")
        sys.exit(1)

    # 处理 data_path，检查是文件还是目录
    data_files = process_data_path(data_path)

    # 初始化 PulsarMessageSender
    sender = PulsarMessageSender(
        pulsar_url=pulsar_url,
        pulsar_port=producer_port,
        full_topic=args.full_topic,
        schema_path=args.schema_path or os.getenv(EnvironmentVariables.PULSAR_SCHEMA_PATH.value),  # 从环境变量获取 schema path
        token=token
    )

    # 遍历数据文件并发送消息
    for data_file in data_files:
        try:
            with open(data_file, 'r') as file:
                message_data = json.load(file)
            sender.send_message(message_data, data_file)
        except Exception as e:
            print(f"Error: Failed to load message data from {data_file}. Exception: {e}")
    
    # 关闭生产者和客户端
    sender.close()


if __name__ == '__main__':
    main()
