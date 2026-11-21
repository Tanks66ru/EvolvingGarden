#!/usr/bin/env python3
"""Convert a 16-bit PCM WAV to the EvolvingGarden sound format."""

from __future__ import annotations

import argparse
import array
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "assets" / "sounds"
TARGET_RATE = 22_050
CUES = (
    "button_click", "reaction_happy", "reaction_sad", "reaction_angry",
    "reaction_fearful", "reaction_surprised", "reaction_curious",
    "reaction_affectionate", "reaction_disgusted",
)


def convert(source: Path, cue: str) -> Path:
    with wave.open(str(source), "rb") as wav:
        if wav.getcomptype() != "NONE" or wav.getsampwidth() != 2:
            raise ValueError("Input must be uncompressed 16-bit PCM WAV.")
        channels = wav.getnchannels()
        if channels not in (1, 2):
            raise ValueError("Input must be mono or stereo.")
        source_rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())

    samples = array.array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()

    if channels == 2:
        mono = array.array("h", (
            max(-32768, min(32767, (samples[i] + samples[i + 1]) // 2))
            for i in range(0, len(samples) - 1, 2)
        ))
    else:
        mono = samples

    if source_rate <= 0:
        raise ValueError("Invalid sample rate.")

    if source_rate != TARGET_RATE and mono:
        count = max(1, round(len(mono) * TARGET_RATE / source_rate))
        result = array.array("h")
        for i in range(count):
            position = i * source_rate / TARGET_RATE
            left = min(int(position), len(mono) - 1)
            right = min(left + 1, len(mono) - 1)
            fraction = position - left
            value = round(mono[left] * (1.0 - fraction) + mono[right] * fraction)
            result.append(max(-32768, min(32767, value)))
        mono = result

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    destination = OUTPUT_DIR / f"{cue}.wav"
    with wave.open(str(destination), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(TARGET_RATE)
        wav.writeframes(mono.tobytes())
    return destination


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("cue", choices=CUES)
    args = parser.parse_args()
    if not args.source.is_file():
        parser.error(f"File not found: {args.source}")
    try:
        print(f"Created: {convert(args.source, args.cue)}")
    except (OSError, wave.Error, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
