"""Research parser for KnowWonder (Brother Bear) UAnimation exports.

Usage: kwanim.py <package> [animation name]. Checks that every animation parses to its exact size."""
import sys, struct
sys.path.insert(0, sys.argv[0].rsplit('/', 1)[0])
from upkg import Package, R

def parse(p, e):
    d = p.data[e['off']:e['off'] + e['size']]
    r = R(d)
    assert r.idx() == 0  # property list terminator (None)
    nb = r.idx()
    bones = [(p.names[r.idx()], r.u32(), r.u32()) for _ in range(nb)]
    nm = r.idx()
    moves = []
    for _ in range(nm):
        m = {}
        m['speed'] = struct.unpack_from('<3f', d, r.p); r.p += 12
        m['tracktime'] = struct.unpack_from('<f', d, r.p)[0]; r.p += 4
        m['startbone'] = r.u32(); m['flags'] = r.u32()
        m['boneidx'] = [r.u32() for _ in range(r.idx())]
        tracks = []
        for _ in range(r.idx()):
            t = dict(flags=r.u32(), nq=r.idx(), np=r.idx(), nt=r.idx())
            t['posscale'], t['timescale'] = struct.unpack_from('<2f', d, r.p); r.p += 8
            tracks.append(t)
        m['tracks'] = tracks
        moves.append(m)
    seqs = []
    for _ in range(r.idx()):
        name = p.names[r.idx()]; group = p.names[r.idx()]
        start = r.u32(); num = r.u32()
        notifys = [(struct.unpack_from('<f', d, r.p)[0], (r.__setattr__('p', r.p + 4), p.names[r.idx()])[1]) for _ in range(r.idx())]
        rate = struct.unpack_from('<f', d, r.p)[0]; r.p += 4
        seqs.append((name, group, start, num, rate, notifys))
    blobstart = r.p
    # Keys for all moves and tracks are stored together after the sequences, in track order.
    nq = r.idx(); quats = [struct.unpack_from('<3h', d, r.p + 6 * i) for i in range(nq)]; r.p += 6 * nq
    npk = r.idx(); pos = [struct.unpack_from('<3h', d, r.p + 6 * i) for i in range(npk)]; r.p += 6 * npk
    nt = r.idx(); times = list(d[r.p:r.p + nt]); r.p += nt
    sums = tuple(sum(t[k] for m in moves for t in m['tracks']) for k in ('nq', 'np', 'nt'))
    keys = dict(quats=quats, pos=pos, times=times)
    return dict(bones=bones, moves=moves, seqs=seqs, blobstart=blobstart, end=r.p, size=len(d), keys=keys, sums=sums, counts=(nq, npk, nt))

if __name__ == '__main__':
    p = Package(sys.argv[1])
    for e in p.exports:
        if p.objname(e['cls']) != 'Animation' or (len(sys.argv) > 2 and e['name'] != sys.argv[2]):
            continue
        try:
            a = parse(p, e)
            ok = a['end'] == a['size'] and a['sums'] == a['counts']
            print(f"{e['name']}: bones {len(a['bones'])} moves {len(a['moves'])} seqs {len(a['seqs'])} end {a['end']}/{a['size']} {'OK' if ok else 'MISMATCH'}")
            if len(sys.argv) > 2 or not ok:
                print('  key counts in tracks', a['sums'], 'stored', a['counts'])
                for m in a['moves'][:3]:
                    print('  move tracktime', m['tracktime'], 'tracks', len(m['tracks']), 'boneidx', len(m['boneidx']))
                for s in a['seqs'][:8]:
                    print('  seq', s)
        except Exception as ex:
            print(e['name'], 'ERROR', ex)
