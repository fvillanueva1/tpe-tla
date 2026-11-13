"""Regenera los fixtures a partir de eventos esperados, sin ejecutar Cadence.

Ejecutar con python3 desde cualquier directorio. La suite compara los archivos
con estos fixtures y nunca los regenera automáticamente.
"""
from pathlib import Path
import struct

ROOT = Path(__file__).parent / "artifacts"


def quantity(value):
    groups = [value & 127]
    while value > 127:
        value >>= 7
        groups.insert(0, 128 | (value & 127))
    return bytes(groups)


def midi(events, tempo=120, instrument=0):
    # (pitch, start, duration, channel); note-offs precede note-ons at a boundary.
    track = b"\0\xff\x51\x03" + (60000000 // tempo).to_bytes(3, "big")
    for channel in sorted({event[3] for event in events}):
        track += bytes([0, 0xC0 | channel, instrument])
    messages = []
    for order, (pitch, start, duration, channel) in enumerate(events):
        messages += [(start, 1, order, channel, pitch), (start + duration, 0, order, channel, pitch)]
    now = 0
    for tick, on, _, channel, pitch in sorted(messages):
        track += quantity(tick - now) + bytes([(0x90 if on else 0x80) | channel, pitch, 64 if on else 0])
        now = tick
    track += b"\0\xff\x2f\0"
    return b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(track)) + track


def blocks(chords, duration=480, voices=False):
    return [(pitch, index * duration, duration, voice if voices else 0)
            for index, chord in enumerate(chords) for voice, pitch in enumerate(chord)]


def sheet(staves, tempo=120):
    text = '\\version "2.24.0"\n\\header { tagline = ##f }\n\\score {\n  <<\n'
    for index, (name, clef, notes) in enumerate(staves):
        text += f'    \\new Staff \\with {{ instrumentName = "{name}" }} {{\n' if name else '    \\new Staff {\n'
        text += f'      \\clef "{clef}"\n'
        if index == 0:
            text += f'      \\tempo 4 = {tempo}\n'
        text += '      ' + notes + '\n    }\n'
    return (text + '  >>\n  \\layout { }\n}\n').encode()


def save(case, name, data):
    directory = ROOT / case
    directory.mkdir(parents=True, exist_ok=True)
    (directory / name).write_bytes(data)


triad = blocks([[60, 64, 67]])
c_scale = blocks([[pitch] for pitch in [60, 62, 64, 65, 67, 69, 71]])
g_scale = blocks([[pitch] for pitch in [67, 69, 71, 72, 74, 76, 78]])
save('30-export-scale', 'escala.mid', midi(c_scale))
save('scale-midi-variable', 'declarada.mid', midi(c_scale))
save('scale-midi-g-major', 'sol.mid', midi(g_scale, instrument=40))
for case in ['48-export-midi', 'export-instrument']:
    save(case, 'tema.mid', midi(triad, tempo=90))
save('semantic-dynamic-values', 'tema.mid', midi(blocks([[60, 64, 67]] * 2), tempo=92))
for case, tempo in [('export-sheet', 120), ('export-sheet-tempo', 90)]:
    save(case, 'tema.ly', sheet([(None, 'treble', "<c' e' g'>4 ")], tempo))
save('sheet-low-tempo', 'tema.ly', sheet([(None, 'treble', '')], 1))

case = 'runtime-export-values'
save(case, 'nota.mid', midi(blocks([[59]]), tempo=100, instrument=73))
save(case, 'acorde.mid', midi(triad))
save(case, 'escala.ly', sheet([(None, 'treble', "g'4 a'4 b'4 c''4 d''4 e''4 fis''4 ")]))
save(case, 'progresion.mid', midi(blocks([[67, 71, 74], [74, 78, 81]]), tempo=90, instrument=40))
save(case, 'voces.mid', midi(blocks([[72, 67, 64, 48], [71, 67, 62, 43]], voices=True)))
save(case, 'voces.ly', sheet([
    ('Soprano', 'treble', "c''4 b'4 "), ('Alto', 'treble', "g'4 g'4 "),
    ('Tenor', 'treble_8', "e'4 d'4 "), ('Bass', 'bass', 'c4 g,4 ')]))
save(case, 'arpegio.mid', midi(blocks([[60], [64], [67]], duration=240)))
save(case, 'arpegio.ly', sheet([(None, 'treble', "c'8 e'8 g'8 ")]))
save(case, 'vacio.mid', midi([]))
save(case, 'nota{final}.mid', midi(blocks([[60]])))
save(case, 'antes.ly', sheet([(None, 'treble', "c'4 ")]))

case = 'runtime-export-snapshot'
save(case, 'antes.ly', sheet([(None, 'treble', "<c' e' g'>4 ")]))
save(case, 'despues.ly', sheet([(None, 'treble', "<d' f' a'>4 ")]))
save(case, 'tempo.ly', sheet([(None, 'treble', "c'4 ")], 60000001))
