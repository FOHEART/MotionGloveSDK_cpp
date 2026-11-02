"""
@file  mock_glove_sender.py
@brief 模拟手套数据发送端：向本机 UDP 端口发送 42 段（或 32 段）CSV 骨骼帧。
@details
帧格式与 MotionGloveSDK/include/MOTIONGLOVE_SDK_STREAM_FORMAT_2026-11-02.md 一致：

  <ActorName> pos quat relative fn <帧号> [skeleton_42] subpackage <x>/<y>,<骨骼数值串>

42 段：每段 3 个位置（米）+ 4 个四元数（线上顺序 wxyz），共 42 x 7 = 294 个数值。
位置让手掌根做正弦运动、手指节点随时间摆动，便于肉眼确认接收端在更新。

用法：
  python tools/mock_glove_sender.py [--port 5001] [--rate 60] [--seconds 10] [--format 42|32]
"""

import argparse
import math
import socket
import sys
import time

SKELETON_42 = 42
SKELETON_32 = 32


def build_frame_values(skeleton_count: int, t: float):
    """@brief 生成一帧的骨骼数值列表（pos + quat wxyz）。

    @param skeleton_count 骨骼段数（42 或 32）。
    @param t 时间（秒），驱动正弦运动。
    @return 位置列表（每段 3 项）与四元数列表（每段 4 项 wxyz）。
    """
    positions = []
    quats = []
    swing = 0.4 * math.sin(2.0 * math.pi * 0.8 * t)   # 手指摆动（模拟抓握）
    wave = 0.03 * math.sin(2.0 * math.pi * 0.5 * t)   # 手掌浮动

    if skeleton_count == SKELETON_42:
        # 槽位 0 = RightHand（手掌根），槽位 21 = LeftHand
        hand_bases = [0, 21]
    else:
        # 32 段：右手手掌在 0，左手手掌在 16
        hand_bases = [0, 16]

    for i in range(skeleton_count):
        base = i % 16 if skeleton_count == SKELETON_32 else i % 21
        if base == 0:
            # 手掌根：随时间波动 + 慢速环绕
            x = 0.05 * math.sin(2.0 * math.pi * 0.3 * t)
            y = 0.10 + wave
            z = 0.05 * math.cos(2.0 * math.pi * 0.3 * t) - 0.05
        else:
            # 手指关节：围绕手掌根的近似球形分布 + 摆动
            ring = base % 5
            seg = (base - 1) // 5 + 1
            x = 0.02 * ring * seg * (1.0 + swing)
            y = 0.10 + wave + 0.02 * seg
            z = 0.01 * seg * swing
        positions.extend([x, y, z])
        # 单位四元数（线上 wxyz），绕 Y 轴小角度旋转
        half = 0.5 * swing * 0.1
        quats.extend([math.cos(half), 0.0, math.sin(half), 0.0])
    return positions, quats


def main():
    parser = argparse.ArgumentParser(description="模拟手套 CSV 数据发送端")
    parser.add_argument("--port", type=int, default=5001)
    parser.add_argument("--rate", type=int, default=60, help="发送帧率 (Hz)")
    parser.add_argument("--seconds", type=float, default=10.0, help="发送时长（秒）")
    parser.add_argument("--format", type=int, choices=(SKELETON_32, SKELETON_42), default=SKELETON_42)
    parser.add_argument("--actor", type=str, default="Glove1-SN-MOCK01", help="数据流名称")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = ("127.0.0.1", args.port)
    fmt_marker = " skeleton_42" if args.format == SKELETON_42 else ""

    print(f"sending {args.format}-skeleton CSV frames to {dest[0]}:{dest[1]} "
          f"at {args.rate} Hz for {args.seconds} s (actor={args.actor})")

    fn = 0
    t0 = time.time()
    next_send = t0
    interval = 1.0 / args.rate
    while time.time() - t0 < args.seconds:
        t = time.time() - t0
        positions, quats = build_frame_values(args.format, t)
        values = positions + quats if False else None  # placeholder (not used)
        # 42 段布局：每段 [pos x y z][quat w x y z] 交错排列
        body_parts = []
        seg_count = args.format
        for i in range(seg_count):
            body_parts.extend(positions[i * 3:i * 3 + 3])
            body_parts.extend(quats[i * 4:i * 4 + 4])
        body = ",".join(f"{v:.6f}" for v in body_parts)
        header = f"{args.actor} pos quat relative fn {fn}{fmt_marker} subpackage 1/1,"
        packet = (header + body).encode("ascii")
        sock.sendto(packet, dest)
        fn += 1
        next_send += interval
        sleep_left = next_send - time.time()
        if sleep_left > 0:
            time.sleep(sleep_left)
        else:
            next_send = time.time()

    print(f"done: {fn} frames sent.")
    sock.close()
    sys.exit(0)


if __name__ == "__main__":
    main()
