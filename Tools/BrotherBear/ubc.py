"""UE1 bytecode walker for KnowWonder (Brother Bear) research.
Parses every Function/State in a package, checks the walked in-memory size
against ScriptSize, and reports unknown tokens with context."""
import sys, collections
sys.path.insert(0, __import__('os').path.dirname(__file__))
from upkg import Package, R

class BadToken(Exception): pass

# Brother Bear inserts one token before GlobalFunction: 0x39-0x46 are the standard
# 0x38-0x45 (GlobalFunction and conversions up to FloatToBool), 0x47+ are standard.
# Every conversion takes one operand, so for walking we treat 0x3A-0x5F alike.
GLOBALFUNC = 0x39
MINCONV, MAXCONV = 0x3A, 0x60
EXTNATIVE, FIRSTNATIVE = 0x60, 0x70

class Walker:
    def __init__(s, pkg, r, extra=None):
        s.p, s.r, s.mem, s.extra = pkg, r, 0, extra or {}
        s.seen = collections.Counter()
    def b(s): s.mem += 1; return s.r.u8()
    def w(s): s.mem += 2; return s.r.u16()
    def d(s): s.mem += 4; return s.r.u32()
    def i(s): s.mem += 4; return s.r.idx()
    def tok(s, depth=0):
        if depth > 64: raise BadToken('depth')
        t = s.b(); s.seen[t] += 1
        if t in s.extra: s.extra[t](s, depth); return t
        if MINCONV <= t < MAXCONV: s.tok(depth+1); return t
        if t >= FIRSTNATIVE:
            while s.tok(depth+1) != 0x16: pass
            return t
        if t >= EXTNATIVE:
            s.b()
            while s.tok(depth+1) != 0x16: pass
            return t
        if t in (0x1b, GLOBALFUNC, 0x1c):
            s.i()
            while s.tok(depth+1) != 0x16: pass
            return t
        T = s.tok
        if t in (0x00, 0x01, 0x02, 0x20, 0x21, 0x29): s.i()
        elif t == 0x04: T(depth+1)
        elif t == 0x05: s.b(); T(depth+1)
        elif t == 0x06: s.w()
        elif t in (0x07, 0x09, 0x18): s.w(); T(depth+1)
        elif t == 0x0a:
            if s.w() != 0xffff: T(depth+1)
        elif t == 0x0c:
            while True:
                n = s.i(); s.d()
                if s.p.names[n] == 'None': break
        elif t in (0x0d, 0x0e, 0x2d): T(depth+1)
        elif t in (0x0f, 0x10, 0x14, 0x1a): T(depth+1); T(depth+1)
        elif t == 0x11:
            for _ in range(4): T(depth+1)
        elif t in (0x12, 0x19): T(depth+1); s.w(); s.b(); T(depth+1)
        elif t in (0x13, 0x2e, 0x36): s.i(); T(depth+1)
        elif t in (0x08, 0x0b, 0x15, 0x16, 0x17, 0x25, 0x26, 0x27, 0x28, 0x2a, 0x30, 0x31): pass
        elif t == 0x1d: s.d()
        elif t == 0x1e: s.d()
        elif t == 0x1f:
            while s.b() != 0: pass
        elif t == 0x22: s.d(); s.d(); s.d()
        elif t == 0x23: s.d(); s.d(); s.d()
        elif t in (0x24, 0x2c): s.b()
        elif t == 0x2b: s.b(); T(depth+1)
        elif t == 0x2f: T(depth+1); s.w()
        elif t in (0x32, 0x33): s.i(); T(depth+1); T(depth+1)
        elif t == 0x34:
            while s.w() != 0: pass
        else: raise BadToken(f'token {t:#04x}')
        return t

def struct_header(pkg, e):
    blob = pkg.data[e['off']:e['off']+e['size']]
    r = R(blob)
    if e['flags'] & 0x02000000:  # HasStack
        r.idx(); r.idx(); r.p += 12; 
    # property block: just expect None
    n = r.idx()
    assert pkg.names[n] == 'None', pkg.names[n]
    r.idx(); r.idx()          # UField super, next
    r.idx(); r.idx()          # ScriptText, Children
    r.idx(); r.u32(); r.u32() # FriendlyName, Line, TextPos
    size = r.u32()
    return r, size

def walk_package(path, extra=None, verbose=False):
    pkg = Package(path); ok = bad = 0; problems = []
    tokens = collections.Counter()
    for k, e in enumerate(pkg.exports, 1):
        cls = pkg.objname(e['cls'])
        if cls not in ('Function', 'State') or e['size'] == 0: continue
        try:
            r, size = struct_header(pkg, e)
        except Exception as ex:
            problems.append((pkg.path(k), f'header {ex}')); bad += 1; continue
        w = Walker(pkg, r, extra); start = r.p
        try:
            while w.mem < size: w.tok()
            if w.mem != size: raise BadToken(f'size {w.mem} != {size}')
            ok += 1
        except (BadToken, IndexError, AssertionError) as ex:
            bad += 1
            ctx = r.d[max(start, r.p-24):r.p+8].hex(' ')
            problems.append((pkg.path(k), f'{ex} at +{r.p-start} mem {w.mem}/{size}: {ctx}'))
        tokens.update(w.seen)
    return ok, bad, problems, tokens

if __name__ == '__main__':
    tot = collections.Counter()
    for path in sys.argv[1:]:
        ok, bad, problems, tokens = walk_package(path)
        tot.update(tokens)
        print(f'{path}: ok={ok} bad={bad}')
        for p, msg in problems[:8]: print('   ', p, msg)
    print('token counts 0x30-0x40:', {hex(k): v for k, v in sorted(tot.items()) if 0x30 <= k <= 0x40})
