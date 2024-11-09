from setuptools import setup, find_packages

setup(
    name="mental1104",  # 包的名称
    version="0.1",     # 版本号
    description="A custom package with useful functions",
    author="mental1104",
    author_email="mental1104@gmail.com",
    packages=find_packages(),  # 自动查找所有子包
    install_requires=[],       # 如果你的包依赖于其他包，可以在这里添加
)