import pytest

from mental1104.utils.bench_tasks import CpuBoundTask, DatasetFactory, IoBoundTask


class TestIoBoundTask:
    @pytest.mark.asyncio
    async def test_io_task_returns_expected_length(self):
        """
        【场景背景】异步 I/O 任务应模拟网络延迟后返回 payload 长度。
        【步骤输入】调用 io_task(delay_ms=1, payload_len=8)。
        【期望输出】await 结果为 8。
        """
        length = await IoBoundTask.io_task(delay_ms=1, payload_len=8)
        assert length == 8

    def test_blocking_io_task(self):
        """
        【场景背景】同步 blocking_io_task 应与异步版本一致。
        【步骤输入】delay_ms=0, payload_len=5。
        【期望输出】返回 5。
        """
        length = IoBoundTask.blocking_io_task(delay_ms=0, payload_len=5)
        assert length == 5


class TestCpuBoundTask:
    def test_rand_payload_length_and_charset(self):
        """
        【场景背景】CPU 任务随机 payload 长度与字符集需满足约束。
        【步骤输入】rand_payload(16)。
        【期望输出】字符串长度 16 且为小写字母。
        """
        payload = CpuBoundTask.rand_payload(16)
        assert len(payload) == 16
        assert payload.islower()

    def test_rand_payload_invalid_length(self):
        """
        【场景背景】负长度应被拒绝。
        【步骤输入】rand_payload(-1)。
        【期望输出】ValueError。
        """
        with pytest.raises(ValueError):
            CpuBoundTask.rand_payload(-1)

    def test_spin_computation(self):
        """
        【场景背景】spin(n) 用于模拟 CPU 计算，应返回平方和。
        【步骤输入】n=3 与 n=0。
        【期望输出】分别为 5 与 0，与数学推导一致。
        """
        # 手算可验证的序列：0^2 + 1^2 + 2^2 = 5
        assert CpuBoundTask.spin(3) == 5
        assert CpuBoundTask.spin(0) == 0

    def test_spin_invalid_iterations(self):
        """
        【场景背景】迭代次数不能为负。
        【步骤输入】spin(-10)。
        【期望输出】ValueError。
        """
        with pytest.raises(ValueError):
            CpuBoundTask.spin(-10)


class TestDatasetFactory:
    def test_build_json_dataset_basic_shape(self):
        """
        【场景背景】DatasetFactory 应生成结构可靠的 JSON 数据集。
        【步骤输入】n_objects=5, payload_repeat=2。
        【期望输出】长度为 5，id 连续，payload 有旋转差异，嵌套字段符合约束。
        """
        dataset = DatasetFactory.build_json_dataset(n_objects=5, payload_repeat=2)
        assert len(dataset) == 5
        first = dataset[0]
        second = dataset[1]
        assert first["id"] == 0 and dataset[-1]["id"] == 4
        assert first["payload"] != second["payload"]  # payload 进行了旋转
        assert isinstance(first["nested"]["values"], list)
        assert len(first["nested"]["tags"]) == 10

    def test_build_json_dataset_invalid_args(self):
        """
        【场景背景】参数越界时应抛异常。
        【步骤输入】n_objects=0 或 payload_repeat=0。
        【期望输出】两次调用均抛 ValueError。
        """
        with pytest.raises(ValueError):
            DatasetFactory.build_json_dataset(0, 1)
        with pytest.raises(ValueError):
            DatasetFactory.build_json_dataset(1, 0)
