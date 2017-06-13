"""Parse blu-ray .mpls
Code from https://gist.github.com/dk00/0a0634c5666cf1b8ab9f
"""
import os
import collections
import io
import struct
import json
import argparse
import vapoursynth as vs


def _int_be(data):
    return {
        1: ord,
        2: lambda b: struct.unpack('>H', b)[0],
        4: lambda b: struct.unpack('>I', b)[0]
    }[len(data)](data)


def load(file, fix_overlap=True):
    # https://github.com/lerks/BluRay/wiki/MPLS
    file.seek(8)
    addr_items, addr_marks = _int_be(file.read(4)), _int_be(file.read(4))
    file.seek(addr_items + 6)
    item_count = _int_be(file.read(2))
    file.seek(2, io.SEEK_CUR)

    def read_item():
        block_size = _int_be(file.read(2))
        item = collections.OrderedDict()
        item['name'] = file.read(5).decode()
        file.seek(7, io.SEEK_CUR)
        item['times'] = [_int_be(file.read(4)), _int_be(file.read(4))]
        file.seek(block_size-20, io.SEEK_CUR)
        return item

    items = [read_item() for _ in range(item_count)]

    file.seek(addr_marks + 4)
    mark_count = _int_be(file.read(2))

    def read_mark():
        file.seek(2, io.SEEK_CUR)
        index = _int_be(file.read(2))
        time = _int_be(file.read(4))
        file.seek(6, io.SEEK_CUR)
        return index, time

    for _ in range(mark_count):
        index, time = read_mark()
        if time > items[index]['times'][-2]:
            items[index]['times'].insert(-1, time)

    if fix_overlap:
        b = None
        for item in items:
            a, b = b, item['times']
            if a and b[0] < a[-1] < b[-1]:
                a[-1] = b[0]
        if len(b) > 1 and b[-1] - b[-2] < 90090:
            b.pop()

    return items


def source(mpls_file, format=None, cache=None, decoder='lsmas'):
    core = vs.get_core()

    if decoder == 'ffms2':
        decoder = core.ffms2.Source
    elif decoder == 'lsmas':
        decoder = core.lsmas.LWLibavSource
    else:
        raise ValueError('decoder must be "ffms2" or "lsmas".')

    stream_dir = os.path.join(os.path.split(os.path.split(mpls_file)[0])[0], 'STREAM')
    clip_list = []
    ts_list = []

    for i in load(open(mpls_file, 'rb')):
        ts_list.append(i['name'])

    for i in ts_list:
        clip_list.append(decoder(os.path.join(stream_dir, i + '.m2ts'), format=format, cache=cache))

    return core.std.Splice(clip_list)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('filename')
    data = load(open(ap.parse_args().filename, 'rb'))
    print(json.dumps(data, indent=2))

if __name__ == '__main__':
    main()
