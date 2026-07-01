#!/usr/bin/env python3
"""Generate placeholder sound effects + a looping music track for Grand Regent.

Pure stdlib (wave/struct/math/random) -- no external deps. Everything is
synthesized (square/saw/tri/noise oscillators), so the game ships with audible
audio out of the box. Replace any file in assets/audio/ with a real recording
of the same name and it just works; no code changes needed.

    python3 tools/gen_audio.py [output_dir]   # default: assets/audio
"""
import math, os, random, struct, sys, wave

RATE = 44100


def _osc(phase, kind):
    x = phase - math.floor(phase)
    if kind == "sine":   return math.sin(2 * math.pi * phase)
    if kind == "square": return 1.0 if x < 0.5 else -1.0
    if kind == "saw":    return 2.0 * x - 1.0
    if kind == "tri":    return 4.0 * abs(x - 0.5) - 1.0
    return 0.0


def _envelope(i, n, atk, rel):
    if i < atk:
        return i / atk
    if i > n - rel:
        return max(0.0, (n - i) / rel)
    return 1.0


def tone(freq, dur, kind="square", vol=0.5, atk=0.005, rel=0.02):
    n = int(dur * RATE)
    a = max(1, int(atk * RATE))
    r = max(1, int(rel * RATE))
    return [vol * _envelope(i, n, a, r) * _osc(freq * i / RATE, kind) for i in range(n)]


def sweep(f0, f1, dur, kind="square", vol=0.5, atk=0.005, rel=0.03):
    n = int(dur * RATE)
    a = max(1, int(atk * RATE))
    r = max(1, int(rel * RATE))
    out, phase = [], 0.0
    for i in range(n):
        phase += (f0 + (f1 - f0) * (i / n)) / RATE
        out.append(vol * _envelope(i, n, a, r) * _osc(phase, kind))
    return out


def noise(dur, vol=0.5, decay=20.0):
    n = int(dur * RATE)
    return [vol * math.exp(-decay * i / n) * (random.random() * 2 - 1) for i in range(n)]


def mix(*layers):
    n = max(len(l) for l in layers)
    out = [0.0] * n
    for l in layers:
        for i, v in enumerate(l):
            out[i] += v
    return out


def normalize(samples, peak=0.85):
    m = max(1e-9, max(abs(s) for s in samples))
    g = peak / m
    return [s * g for s in samples]


def write_wav(path, samples, rate=RATE):
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(
            struct.pack("<h", int(max(-32768, min(32767, s * 32767)))) for s in samples))


def midi(n):
    return 440.0 * 2 ** ((n - 69) / 12.0)


def build_music():
    """~8s looping chiptune: Am - F - C - G, with bass + arpeggio + melody."""
    eighth = 0.25  # seconds (120 BPM)
    prog = [([57, 60, 64], 45),   # Am, bass A2
            ([53, 57, 60], 41),   # F,  bass F2
            ([48, 52, 55], 36),   # C,  bass C2
            ([55, 59, 62], 43)]   # G,  bass G2
    melody = [69, 72, 76, 72,  74, 76, 72, None,
              65, 69, 72, 69,  72, 74, 72, None,
              64, 67, 72, 67,  76, 72, 67, None,
              67, 71, 74, 71,  74, 71, 67, None]
    step = int(eighth * RATE)
    bass, arp, mel = [], [], []
    for ci, (tones, root) in enumerate(prog):
        for e in range(8):
            bass += tone(midi(root), eighth, "tri", 0.5, rel=0.03)
            a = tone(midi(tones[e % len(tones)]), eighth * 0.9, "square", 0.16, rel=0.04)
            arp += a + [0.0] * (step - len(a))
            m = melody[ci * 8 + e]
            if m is None:
                mel += [0.0] * step
            else:
                t = tone(midi(m), eighth * 0.95, "square", 0.30, rel=0.05)
                mel += t + [0.0] * (step - len(t))
    return normalize(mix(bass, arp, mel), 0.8)


def build_sfx():
    return {
        "jump":         normalize(sweep(300, 720, 0.14, "square", 0.6)),
        "attack":       normalize(mix(sweep(880, 300, 0.11, "saw", 0.4), noise(0.11, 0.35, 35))),
        "skill":        normalize(mix(sweep(180, 540, 0.26, "saw", 0.5), sweep(90, 260, 0.26, "square", 0.3))),
        "hurt":         normalize(sweep(520, 150, 0.20, "square", 0.6)),
        "enemy_hit":    normalize(mix(noise(0.07, 0.5, 45), tone(180, 0.05, "square", 0.3))),
        "enemy_death":  normalize(mix(sweep(420, 80, 0.30, "saw", 0.55), noise(0.30, 0.25, 12))),
        "player_death": normalize(sweep(300, 55, 0.90, "tri", 0.6, rel=0.2)),
        "victory":      normalize(tone(midi(72), 0.14, "square", 0.5) + tone(midi(76), 0.14, "square", 0.5)
                                  + tone(midi(79), 0.14, "square", 0.5) + tone(midi(84), 0.32, "square", 0.5)),
    }


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "assets/audio"
    os.makedirs(out_dir, exist_ok=True)
    for name, samples in build_sfx().items():
        path = os.path.join(out_dir, name + ".wav")
        write_wav(path, samples)
        print("wrote", path)
    music_path = os.path.join(out_dir, "music.wav")
    write_wav(music_path, build_music())
    print("wrote", music_path)


if __name__ == "__main__":
    main()
