"""`pip install -e agent-harness` 后可用 `motionglovesdk-cli` 入口命令。"""

from pathlib import Path

from setuptools import find_packages, setup

ROOT = Path(__file__).resolve().parent

setup(
    name="cli-anything-motionglovesdk",
    version="0.2.1",
    description="CLI-Anything harness for MotionGloveSDK (motionGloveSDK_cli)",
    long_description=(ROOT.parent / "SKILL.md").read_text(encoding="utf-8")
    if (ROOT.parent / "SKILL.md").is_file()
    else "",
    long_description_content_type="text/markdown",
    packages=find_packages(include=["cli_anything", "cli_anything.*"]),
    include_package_data=True,
    python_requires=">=3.8",
    install_requires=["click>=8.0"],
    extras_require={"test": ["pytest>=7.0"]},
    entry_points={
        "console_scripts": [
            "motionglovesdk-cli=cli_anything.motionglovesdk._cli:main",
        ],
    },
    classifiers=[
        "Programming Language :: Python :: 3",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
    ],
)
