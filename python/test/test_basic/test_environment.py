import pytest
import os
from mental1104 import check_required_env_vars, MissingEnvVarError

class TestEnvironment:
    @pytest.fixture
    def mock_env(self, mocker):
        """模拟 os.environ 的 Fixture"""
        return mocker.patch.dict(os.environ, clear=True)

    def test_check_required_env_vars_all_present(self, mock_env):
        # 准备测试数据：所有环境变量存在
        required_env_vars = ["ENV_VAR_1", "ENV_VAR_2"]
        mock_env.update({var: "value" for var in required_env_vars})

        # 调用测试方法，验证没有抛出异常
        try:
            check_required_env_vars(required_env_vars)
        except MissingEnvVarError:
            pytest.fail("MissingEnvVarError raised unexpectedly")

    def test_check_required_env_vars_missing_var(self, mock_env):
        # 准备测试数据：部分环境变量缺失
        required_env_vars = ["ENV_VAR_1", "ENV_VAR_2"]
        mock_env.update({"ENV_VAR_1": "value"})  # 只设置了一个变量

        # 调用测试方法，验证是否抛出 MissingEnvVarError 异常
        with pytest.raises(MissingEnvVarError, match="Missing required environment variables: ENV_VAR_2"):
            check_required_env_vars(required_env_vars)