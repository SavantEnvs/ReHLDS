#!/usr/bin/env python3
"""Regenerates the committed seed corpora under mayhem/<target>/testsuite/ (stdlib only)."""
import bz2
import os
import random
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
TABLE2 = [0x05, 0x61, 0x7A, 0xED, 0x1B, 0xCA, 0x0D, 0x9B, 0x4A, 0xF1, 0x64, 0xC7, 0xB5, 0x8E, 0xDF, 0xA0]


def put(target, name, data):
    d = os.path.join(HERE, target, "testsuite")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "wb") as f:
        f.write(data)


# ---- fuzz_bsp: Half-Life BSP v30 ------------------------------------------------------------
def lump_data():
    planes = struct.pack("<3ffi", 1, 0, 0, 0.0, 0) + struct.pack("<3ffi", 0, 1, 0, 0.0, 1)
    vertexes = b"".join(struct.pack("<3f", *v) for v in [(0, 0, 0), (64, 0, 0), (0, 64, 0)])
    edges = b"".join(struct.pack("<2H", *e) for e in [(0, 0), (0, 1), (1, 2), (2, 0)])
    surfedges = struct.pack("<3i", 1, 2, 3)
    miptex = struct.pack("<16s2I4I", b"seed", 16, 16, 0, 0, 0, 0)
    textures = struct.pack("<2i", 1, 12) + miptex
    texinfo = struct.pack("<8f2i", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0)
    faces = struct.pack("<2hi2h4Bi", 0, 0, 0, 3, 0, 255, 255, 255, 255, -1)
    lighting = bytes([128, 128, 128] * 4)
    marksurfaces = struct.pack("<H", 0)
    # leaf 0 is the solid "outside" leaf; nodes address leaves as -1-n.
    leafs = b"".join(
        struct.pack("<ii3h3hHH4B", contents, visofs, -64, -64, -64, 64, 64, 64, 0, 0, 0, 0, 0, 0)
        for contents, visofs in [(-2, -1), (-1, 0), (-3, 0)]
    )
    nodes = struct.pack("<i2h3h3hHH", 0, 1, -2, -64, -64, -64, 64, 64, 64, 0, 0) + struct.pack(
        "<i2h3h3hHH", 1, -3, -1, -64, -64, -64, 64, 64, 64, 0, 0
    )
    clipnodes = struct.pack("<i2h", 0, 1, -2) + struct.pack("<i2h", 1, -3, -1)
    visibility = bytes([0xFF, 0x01, 0x00, 0x02])
    entities = b'{\n"classname" "worldspawn"\n"wad" "halflife.wad"\n}\n\0'
    return {
        0: entities, 1: planes, 2: textures, 3: vertexes, 4: visibility, 5: nodes, 6: texinfo, 7: faces,
        8: lighting, 9: clipnodes, 10: leafs, 11: marksurfaces, 12: edges, 13: surfedges, 14: b"",
    }


def build_bsp(keep):
    lumps = lump_data()
    body = b""
    table = []
    off = 4 + 15 * 8
    for i in range(15):
        blob = lumps[i] if i in keep else b""
        pad = (-len(blob)) % 4
        table.append((off if blob else 0, len(blob)))
        body += blob + b"\0" * pad
        off += len(blob) + pad
    hdr = struct.pack("<i", 30) + b"".join(struct.pack("<2i", o, n) for o, n in table)
    return hdr + body


# ---- fuzz_netchan -----------------------------------------------------------------------------
def munge2(buf, seq):
    seq &= 0xFFFFFFFF
    out = bytearray(buf)
    for i in range(len(out) // 4):
        c = struct.unpack_from("<I", out, i * 4)[0]
        c ^= (~seq) & 0xFFFFFFFF
        c = struct.unpack(">I", struct.pack("<I", c))[0]
        bs = bytearray(struct.pack("<I", c))
        for j in range(4):
            bs[j] ^= (0xA5 | ((j << j) & 0xFF) | j | TABLE2[(i + j) & 0x0F]) & 0xFF
        c = struct.unpack("<I", bytes(bs))[0] ^ seq
        struct.pack_into("<I", out, i * 4, c)
    return bytes(out)


def packet(seq, ack, body, reliable=False, frag=None):
    flags = seq | ((1 << 31) if reliable else 0)
    if frag is not None:
        flags |= 1 << 30
        fragid, payload = frag
        body = struct.pack("<BIhh", 1, fragid, 0, len(payload)) + b"\0" + payload
    raw = struct.pack("<II", flags, ack) + munge2(body, seq & 0xFF)
    return raw


def frame(*packets):
    return b"".join(struct.pack("<H", len(p)) + p for p in packets)


def fragid(i, n):
    return ((i & 0xFFFF) << 16) | (n & 0xFFFF)


def netchan_seeds():
    put("fuzz_netchan", "connectionless", frame(b"\xff\xff\xff\xffgetchallenge valve\n"))
    put("fuzz_netchan", "reliable_plain", frame(packet(1, 0, b"\x05hello world, hltv\0\0\0\0", reliable=True)))
    msg = b"A" * 120
    put("fuzz_netchan", "one_fragment", frame(packet(1, 0, b"", frag=(fragid(1, 1), msg))))
    put("fuzz_netchan", "two_fragments", frame(
        packet(1, 0, b"", frag=(fragid(1, 2), msg[:60])),
        packet(2, 0, b"", frag=(fragid(2, 2), msg[60:])),
    ))
    z = b"BZ2\0" + bz2.compress(b"\x01svc_print\0" * 40)
    put("fuzz_netchan", "bz2_fragments", frame(
        packet(1, 0, b"", frag=(fragid(1, 2), z[: len(z) // 2])),
        packet(2, 0, b"", frag=(fragid(2, 2), z[len(z) // 2:])),
    ))


# ---- text / bit targets -----------------------------------------------------------------------
def small_seeds():
    put("fuzz_infostring", "userinfo", b"\\name\\player\\model\\gordon\\rate\\9999\\cl_updaterate\\30\\_vgui_menus\\1")
    put("fuzz_infostring", "serverinfo", b"\\*gamedir\\valve\\hostname\\rehlds\\mapname\\crossfire\\*sid\\123456\\")
    put("fuzz_infostring", "dangling", b"\\key\\\\empty\\\\novalue")
    put("fuzz_tokenline", "connect", b'connect 127.0.0.1:27015 -port 27020 "quoted arg" +map de_dust2')
    put("fuzz_tokenline", "console", b'record "my demo"; bind\tx\t+attack  \x01\x7f tail')
    put("fuzz_tokenline", "multiline", b'connect 1.2.3.4\n"open quote\nrecord demo -port 1 -port\n\n   \t \nquit')
    put("fuzz_tokenline", "many_tokens", b" ".join(b"t%d" % n for n in range(127)) + b"\n" + b" ".join(b"x" for _ in range(100)))
    put("fuzz_tokenline", "long_line", b"A" * 2046 + b"\n" + b"A" * 2047 + b"\n" + b'"' + b"q" * 2040 + b"\n+map x")
    put("fuzz_tokenline", "quotes", b'"" "a" "b c" ""\n\"unterminated\n"x"y"z" -port "" -port')
    rnd = random.Random(4218)
    for n in range(3):
        put("fuzz_bitbuffer", "ops%d" % n, bytes(rnd.randrange(256) for _ in range(96 + 64 * n)))


if __name__ == "__main__":
    full = set(range(15))
    put("fuzz_bsp", "full.bsp", build_bsp(full))
    put("fuzz_bsp", "tree_only.bsp", build_bsp({1, 4, 5, 10}))
    put("fuzz_bsp", "no_textures.bsp", build_bsp(full - {2, 6, 7, 11}))
    netchan_seeds()
    small_seeds()
