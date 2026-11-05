"""让未安装的 harness 也能直接 `pytest agent-harness`（导入 cli_anything）。"""

import sys
from pathlib import Path

HARNESS_ROOT = Path(__file__).resolve().parent
if str(HARNESS_ROOT) not in sys.path:
    sys.path.insert(0, str(HARNESS_ROOT))
