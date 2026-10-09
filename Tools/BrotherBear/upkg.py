"""Minimal UE1 package table reader (names/imports/exports) for format research."""
import struct, sys

class R:
    def __init__(s, d, p=0): s.d, s.p = d, p
    def u8(s): v = s.d[s.p]; s.p += 1; return v
    def i32(s): v = struct.unpack_from('<i', s.d, s.p)[0]; s.p += 4; return v
    def u32(s): v = struct.unpack_from('<I', s.d, s.p)[0]; s.p += 4; return v
    def u16(s): v = struct.unpack_from('<H', s.d, s.p)[0]; s.p += 2; return v
    def idx(s):
        b0 = s.u8(); neg = b0 & 0x80; v = b0 & 0x3f
        if b0 & 0x40:
            sh = 6
            while True:
                b = s.u8(); v |= (b & 0x7f) << sh; sh += 7
                if not (b & 0x80) or sh > 27: break
        return -v if neg else v
    def string(s, ver):
        if ver >= 64:
            n = s.idx(); raw = s.d[s.p:s.p+n]; s.p += n; return raw.rstrip(b'\0').decode('latin1')
        e = s.d.index(b'\0', s.p); r = s.d[s.p:e].decode('latin1'); s.p = e + 1; return r

class Package:
    def __init__(s, path):
        d = open(path, 'rb').read(); s.data = d; r = R(d)
        assert r.u32() == 0x9E2A83C1
        s.ver = r.u16(); s.lic = r.u16(); s.flags = r.u32()
        nc, no, ec, eo, ic, io = (r.u32() for _ in range(6))
        r.p = no; s.names = []
        for _ in range(nc): s.names.append(r.string(s.ver)); r.u32()
        r.p = io; s.imports = []
        for _ in range(ic):
            s.imports.append(dict(cpkg=s.names[r.idx()], cname=s.names[r.idx()], outer=r.i32(), name=s.names[r.idx()]))
        r.p = eo; s.exports = []
        for _ in range(ec):
            e = dict(cls=r.idx(), sup=r.idx(), outer=r.i32(), name=s.names[r.idx()], flags=r.u32(), size=r.idx())
            e['off'] = r.idx() if e['size'] > 0 else 0
            s.exports.append(e)
    def objname(s, i):
        if i == 0: return 'Class'
        if i < 0: return s.imports[-i - 1]['name']
        return s.exports[i - 1]['name']
    def path(s, i):
        parts = []
        while i:
            parts.append(s.objname(i))
            i = (s.imports[-i - 1]['outer'] if i < 0 else s.exports[i - 1]['outer'])
        return '.'.join(reversed(parts))

if __name__ == '__main__':
    p = Package(sys.argv[1])
    print(f'ver {p.ver} lic {p.lic} names {len(p.names)} imports {len(p.imports)} exports {len(p.exports)}')
    flt = sys.argv[2] if len(sys.argv) > 2 else None
    for i, e in enumerate(p.exports, 1):
        line = f"{i:6d} {p.objname(e['cls']):20s} {p.path(i):60s} size={e['size']} off={e['off']} flags={e['flags']:#x}"
        if not flt or flt in line: print(line)
