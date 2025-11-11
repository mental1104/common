import pytest

from mental1104.utils.bench_tasks import CpuBoundTask, DatasetFactory, IoBoundTask


class TestIoBoundTask:
    @pytest.mark.asyncio
    async def test_io_task_returns_expected_length(self):
        length = await IoBoundTask.io_task(delay_ms=1, payload_len=8)
        assert length == 8

    def test_blocking_io_task(self):
        length = IoBoundTask.blocking_io_task(delay_ms=0, payload_len=5)
        assert length == 5


class TestCpuBoundTask:
    def test_rand_payload_length_and_charset(self):
        payload = CpuBoundTask.rand_payload(16)
        assert len(payload) == 16
        assert payload.islower()

    def test_rand_payload_invalid_length(self):
        with pytest.raises(ValueError):
            CpuBoundTask.rand_payload(-1)

    def test_spin_computation(self):
        # 手算可验证的序列：0^2 + 1^2 + 2^2 = 5
        assert CpuBoundTask.spin(3) == 5
        assert CpuBoundTask.spin(0) == 0

    def test_spin_invalid_iterations(self):
        with pytest.raises(ValueError):
            CpuBoundTask.spin(-10)


class TestDatasetFactory:
    def test_build_json_dataset_basic_shape(self):
        dataset = DatasetFactory.build_json_dataset(n_objects=5, payload_repeat=2)
        assert len(dataset) == 5
        first = dataset[0]
        second = dataset[1]
        assert first["id"] == 0 and dataset[-1]["id"] == 4
        assert first["payload"] != second["payload"]  # payload 进行了旋转
        assert isinstance(first["nested"]["values"], list)
        assert len(first["nested"]["tags"]) == 10

    def test_build_json_dataset_invalid_args(self):
        with pytest.raises(ValueError):
            DatasetFactory.build_json_dataset(0, 1)
        with pytest.raises(ValueError):
            DatasetFactory.build_json_dataset(1, 0)
