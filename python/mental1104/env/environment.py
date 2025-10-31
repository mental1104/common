import os

class MissingEnvVarError(Exception):
    pass


def check_required_env_vars(required_env_vars):
    """
    检查是否具有给定的环境变量，若没有，则抛出异常。

    :param required_env_vars: 需要检查的环境变量列表
    """
    missing_vars = []
    for var in required_env_vars:
        if var not in os.environ:
            missing_vars.append(var)
    
    if missing_vars:
        raise MissingEnvVarError(f"Missing required environment variables: {', '.join(missing_vars)}")