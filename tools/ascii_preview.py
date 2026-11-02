"""ASCII 预览：把截图转成 96x64 灰度字符画，用于确认画面布局。"""
import subprocess
import sys

src = sys.argv[1]
FF = r"D:\autotools\ffmpeg\bin\ffmpeg.exe"
W, H = 96, 64
raw = subprocess.check_output(
    [FF, "-loglevel", "error", "-i", src, "-vf", f"scale={W}:{H}",
     "-f", "rawvideo", "-pix_fmt", "gray", "-"],
)
chars = " .':-=+*#%@"
rows = []
for y in range(H):
    line = ""
    for x in range(W):
        v = raw[y * W + x]
        line += chars[min(len(chars) - 1, v * len(chars) // 256)]
    rows.append(line)
print("\n".join(rows))