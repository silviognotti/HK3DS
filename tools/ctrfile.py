#!/usr/bin/env python3
"""Reads the CIA / CXI files our Makefile builds (makerom, unencrypted): the NCCH's exheader, the ExeFS .code (BLZ
decompressed when the exheader says it's compressed) and the RomFS file list. Used by make_patch.py.
   python ctrfile.py info FILE.cia|FILE.cxi
   python ctrfile.py code FILE OUT_code.bin OUT_exheader.bin     (decompressed .code; exheader info, 0x400 bytes)
   python ctrfile.py romfs FILE OUT_manifest.txt                 (one line per file: sha1 size path)
"""

import hashlib, struct, sys

MU = 0x200  # NCCH media unit


def align(x, a):
    return (x + a - 1) // a * a


def ncch_offset(f):
    """offset of the (first) NCCH in a CIA, or 0 for a bare CXI"""
    f.seek(0)
    h = f.read(0x20)
    if (
        h[0x100 - 0x100 : 4] != b"NCCH" and struct.unpack_from("<I", h, 0)[0] == 0x2020
    ):  # CIA header size
        hdr_size, typ, ver, cert, tik, tmd, meta = struct.unpack_from("<IHHIIII", h, 0)
        off = align(hdr_size, 64) + align(cert, 64) + align(tik, 64) + align(tmd, 64)
        # the TMD's first content chunk says whether content 0 is encrypted
        f.seek(align(hdr_size, 64) + align(cert, 64) + align(tik, 64))
        sigtype = struct.unpack(">I", f.read(4))[0]
        sigsize = {0x10003: 0x200 + 0x3C, 0x10004: 0x100 + 0x3C, 0x10005: 0x3C + 0x40}[
            sigtype
        ]
        tmd0 = align(hdr_size, 64) + align(cert, 64) + align(tik, 64)
        f.seek(tmd0 + 4 + sigsize + 0xC4 + 0x40 * 0x24)
        cid, cidx, ctype, csize = struct.unpack(">IHHQ", f.read(16))
        if ctype & 1:
            sys.exit("content 0 is encrypted: not supported")
        return off
    f.seek(0x100)
    if f.read(4) == b"NCCH":
        return 0
    sys.exit("not a CIA or NCCH")


class NCCH:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.base = ncch_offset(self.f)
        self.f.seek(self.base)
        h = self.f.read(0x200)
        assert h[0x100:0x104] == b"NCCH"
        self.flags = h[0x188:0x190]
        if not self.flags[7] & 4:
            sys.exit("NCCH is encrypted (NoCrypto flag not set): not supported")
        self.tid = struct.unpack_from("<Q", h, 0x118)[0]
        self.exh_size = struct.unpack_from("<I", h, 0x180)[0]
        self.exefs_off, self.exefs_size = (
            v * MU for v in struct.unpack_from("<II", h, 0x1A0)
        )
        self.romfs_off, self.romfs_size = (
            v * MU for v in struct.unpack_from("<II", h, 0x1B0)
        )

    def read(self, off, n):
        self.f.seek(self.base + off)
        return self.f.read(n)

    def exheader(self):
        return self.read(0x200, 0x400)

    def code(self):
        hdr = self.read(self.exefs_off, 0x200)
        for i in range(10):
            name = hdr[i * 16 : i * 16 + 8].rstrip(b"\0")
            off, size = struct.unpack_from("<II", hdr, i * 16 + 8)
            if name == b".code":
                data = self.read(self.exefs_off + 0x200 + off, size)
                return blz_decompress(data) if self.exheader()[0xD] & 1 else data
        sys.exit("no .code in ExeFS")

    def romfs_files(self):
        """(path, offset in the NCCH, size) of every RomFS file"""
        ivfc = self.read(self.romfs_off, 0x60)
        assert ivfc[:4] == b"IVFC"
        master = struct.unpack_from("<I", ivfc, 0x08)[0]
        bs3 = 1 << struct.unpack_from("<I", ivfc, 0x4C)[0]  # level 3 block size (log2)
        l3 = self.romfs_off + align(
            0x60 + master, bs3
        )  # level 3 follows the header and master hash
        hdr = self.read(l3, 0x28)
        vals = struct.unpack_from("<10I", hdr, 0)
        dir_meta_off, dir_meta_size, file_meta_off, file_meta_size, file_data_off = (
            vals[3],
            vals[4],
            vals[7],
            vals[8],
            vals[9],
        )
        dmeta = self.read(l3 + dir_meta_off, dir_meta_size)
        fmeta = self.read(l3 + file_meta_off, file_meta_size)
        out = []

        def dir_name(o):
            nl = struct.unpack_from("<I", dmeta, o + 0x14)[0]
            return dmeta[o + 0x18 : o + 0x18 + nl].decode("utf-16le")

        def walk(o, prefix):
            parent, sib, child, first_file, nxt, nl = struct.unpack_from(
                "<6I", dmeta, o
            )
            fo = first_file
            while fo != 0xFFFFFFFF:
                p, fsib, doff, dsize, nxtf, fnl = struct.unpack_from(
                    "<IIQQII", fmeta, fo
                )
                name = fmeta[fo + 0x20 : fo + 0x20 + fnl].decode("utf-16le")
                out.append((prefix + name, l3 + file_data_off + doff, dsize))
                fo = fsib
            co = child
            while co != 0xFFFFFFFF:
                walk(co, prefix + dir_name(co) + "/")
                co = struct.unpack_from("<I", dmeta, co + 4)[0]

        walk(0, "")
        return out


def blz_decompress(buf):
    """the 3DS ExeFS .code compression (backwards LZ77)"""
    top_bottom, inc = struct.unpack_from("<II", buf, len(buf) - 8)
    if inc == 0:
        return bytes(buf)
    enc_len, hdr_len = top_bottom & 0xFFFFFF, top_bottom >> 24
    out = bytearray(buf) + bytearray(inc)
    src = len(buf) - hdr_len
    end = len(buf) - enc_len
    dst = len(out)
    while src > end:
        src -= 1
        flags = out[src]
        for _ in range(8):
            if src <= end:
                break
            if flags & 0x80:
                src -= 2
                seg = out[src] | out[src + 1] << 8
                n = ((seg >> 12) & 0xF) + 3
                disp = (seg & 0xFFF) + 3
                for _ in range(n):
                    dst -= 1
                    out[dst] = out[dst + disp]
            else:
                src -= 1
                dst -= 1
                out[dst] = out[src]
            flags = (flags << 1) & 0xFF
    return bytes(out)


def main():
    cmd, path = sys.argv[1], sys.argv[2]
    n = NCCH(path)
    if cmd == "info":
        exh = n.exheader()
        text_size, ro_size, data_size = (
            struct.unpack_from("<I", exh, o)[0] for o in (0x18, 0x28, 0x38)
        )
        print(
            f"title id {n.tid:016X}  exefs {n.exefs_size} B  romfs {n.romfs_size} B  name {exh[:8].rstrip(bytes(1)).decode()}"
            f"  code compressed {bool(exh[0xD] & 1)}  text {text_size:#x} ro {ro_size:#x} data {data_size:#x}"
        )
    elif cmd == "code":
        open(sys.argv[3], "wb").write(n.code())
        open(sys.argv[4], "wb").write(n.exheader())
    elif cmd == "romfs":
        with open(sys.argv[3], "w") as o:
            for p, off, size in sorted(n.romfs_files()):
                h = hashlib.sha1()
                left = size
                n.f.seek(n.base + off)
                while left:
                    c = n.f.read(min(left, 1 << 20))
                    h.update(c)
                    left -= len(c)
                o.write(f"{h.hexdigest()} {size} {p}\n")


if __name__ == "__main__":
    main()
