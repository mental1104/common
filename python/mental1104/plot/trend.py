import os
import csv
import matplotlib.pyplot as plt
from collections.abc import Iterable
from datetime import datetime
from mental1104.timed import parse_time

class TrendPlotBase:
    """
    基于 matplotlib 创建趋势图的基类，支持从 CSV 文件和可迭代对象中生成趋势图。
    """

    def __init__(self):
        pass

    @staticmethod
    def plot_from_csv(input_file_path, output_file_path=None):
        """
        从 CSV 文件生成趋势图。

        Args:
            input_file_path (str): 输入的 CSV 文件路径。
            output_file_path (str, optional): 输出的图片文件路径，默认为 None。
        """
        with open(input_file_path, mode='r', encoding='utf-8') as file:
            reader = csv.DictReader(file)
            data = {key: [] for key in reader.fieldnames}

            for row in reader:
                for key, value in row.items():
                    try:
                        data[key].append(float(value))
                    except ValueError:
                        data[key].append(0.0)

        TrendPlotBase._plot_data(data, output_file_path)

    @staticmethod
    def plot_from_iterable(data_iterable, output_file_path=None):
        """
        从可迭代对象生成趋势图。

        Args:
            data_iterable (iterable): 输入的可迭代对象，可以是列表、元组或字典。
            output_file_path (str, optional): 输出的图片文件路径，默认为 None。
        """
        if not isinstance(data_iterable, Iterable):
            raise TypeError("data_iterable 必须是可迭代对象。")

        # 确定列名和数据结构
        if isinstance(data_iterable[0], dict):
            fieldnames = list(data_iterable[0].keys())
            data = {key: [] for key in fieldnames}

            for row in data_iterable:
                for key in fieldnames:
                    elem = row.get(key, 0.0)
                    elem = float(elem) if isinstance(elem, (int, float)) else 0.0
                    data[key].append(elem)

        elif isinstance(data_iterable[0], (list, tuple)):
            max_len = len(data_iterable[0])
            data = {f"Column{i+1}": [] for i in range(max_len)}

            for row in data_iterable:
                for i in range(max_len):
                    try:
                        data[f"Column{i+1}"].append(float(row[i]))
                    except (IndexError, ValueError):
                        data[f"Column{i+1}"].append(0.0)

        else:
            raise TypeError("data_iterable 的元素必须是字典、列表或元组。")

        TrendPlotBase._plot_data(data, output_file_path)

    @staticmethod
    def _plot_data(data, output_file_path):
        """
        绘制趋势图并保存到文件。

        Args:
            data (dict): 包含列名和对应数据的字典。
            output_file_path (str): 输出的图片文件路径。
        """
        if output_file_path is None:
            timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
            output_file_path = os.path.join("/tmp", f"trend_{timestamp}.png")

        elif os.path.isdir(output_file_path):
            timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
            output_file_path = os.path.join(output_file_path, f"trend_{timestamp}.png")

        elif os.path.exists(output_file_path):
            print(f"文件已存在，未覆盖: {output_file_path}")
            return

        elif not os.path.exists(os.path.dirname(output_file_path)):
            os.makedirs(os.path.dirname(output_file_path), exist_ok=True)

        # 开始绘图
        plt.figure(figsize=(10, 6))

        for key, values in data.items():
            plt.plot(range(1, len(values) + 1), values, label=key)

        plt.xlabel("Row Number")
        plt.ylabel("Value")
        plt.title("Trend Plot")
        plt.legend()
        plt.grid(True)

        # 保存图像
        plt.savefig(output_file_path)
        plt.close()

        print(f"趋势图已保存到: {output_file_path}")


class TimeBasedTrendPlot(TrendPlotBase):
    """
    基于时间轴的趋势图派生类。
    """
    
    @staticmethod
    def plot_from_csv(input_file_path, output_file_path=None):
        """
        从 CSV 文件生成基于时间轴的趋势图。

        Args:
            input_file_path (str): 输入的 CSV 文件路径。
            output_file_path (str, optional): 输出的图片文件路径，默认为 None。
        """
        with open(input_file_path, mode='r', encoding='utf-8') as file:
            reader = csv.DictReader(file)

            if 'time' not in reader.fieldnames:
                raise KeyError("CSV 文件中必须包含 'time' 列。")

            data = {key: [] for key in reader.fieldnames if key != 'time'}
            time_values = []

            for row in reader:
                time_str = row.pop('time')
                time_values.append(parse_time(time_str))

                for key, value in row.items():
                    try:
                        data[key].append(float(value))
                    except ValueError:
                        data[key].append(0.0)

        TimeBasedTrendPlot._plot_data_with_time(data, time_values, output_file_path)

    @staticmethod
    def plot_from_iterable(data_iterable, output_file_path=None):
        """
        从可迭代对象生成基于时间轴的趋势图。

        Args:
            data_iterable (iterable): 输入的可迭代对象，可以是字典的队列或列表。
            output_file_path (str, optional): 输出的图片文件路径，默认为 None。
        """
        if not isinstance(data_iterable, Iterable):
            raise TypeError("data_iterable 必须是可迭代对象。")

        if not isinstance(data_iterable[0], dict):
            raise TypeError("data_iterable 的第一个元素必须是字典。")

        if "time" not in data_iterable[0]:
            raise KeyError("数据的第一个字典元素中必须包含 'time' 键。")

        # 验证所有元素
        for i, row in enumerate(data_iterable):
            if i == 0:
                continue  # 跳过第一个元素

            if not isinstance(row, dict) or "time" not in row:
                raise KeyError(f"第 {i+1} 个元素缺少 'time' 键。")

        # 提取时间和其他数据
        data = {}
        time_values = []

        for row in data_iterable:
            time_str = row.pop("time")
            time_values.append(parse_time(time_str))

            for key, value in row.items():
                if key not in data:
                    data[key] = []
                try:
                    data[key].append(float(value))
                except ValueError:
                    data[key].append(0.0)

        TimeBasedTrendPlot._plot_data_with_time(data, time_values, output_file_path)

    @staticmethod
    def _plot_data_with_time(data, time_values, output_file_path):
        """
        绘制基于时间轴的趋势图并保存到文件。

        Args:
            data (dict): 包含列名和对应数据的字典。
            time_values (list): 时间轴数据。
            output_file_path (str): 输出的图片文件路径。
        """
        if output_file_path is None:
            timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
            output_file_path = os.path.join("/tmp", f"trend_{timestamp}.png")

        elif os.path.isdir(output_file_path):
            timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
            output_file_path = os.path.join(output_file_path, f"trend_{timestamp}.png")

        elif os.path.exists(output_file_path):
            print(f"文件已存在，未覆盖: {output_file_path}")
            return

        elif not os.path.exists(os.path.dirname(output_file_path)):
            os.makedirs(os.path.dirname(output_file_path), exist_ok=True)

        # 开始绘图
        plt.figure(figsize=(10, 6))

        for key, values in data.items():
            plt.plot(time_values, values, label=key)

        plt.xlabel("Time")
        plt.ylabel("Value")
        plt.title("Time-Based Trend Plot")
        plt.legend()
        plt.grid(True)

        # 保存图像
        plt.gcf().autofmt_xdate()  # 自动格式化日期显示
        plt.savefig(output_file_path)
        plt.close()

        print(f"时间趋势图已保存到: {output_file_path}")