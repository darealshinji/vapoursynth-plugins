#!/usr/bin/env python3
import json
import urllib3
import certifi

__version__ = '0.1.0'


def main():
    apiroot = 'https://www.wanikani.com/api/v2'
    key = ''

    http = urllib3.PoolManager(cert_reqs='CERT_REQUIRED', ca_certs=certifi.where())
    headers = {'Authorization': 'Token token={}'.format(key)}

    for level in range(1, 61):
        query = '{}/subjects?type=kanji&levels={}'.format(apiroot, level)
        request = http.request('GET', query, headers=headers)

        data = json.loads(request.data.decode('utf-8'))

        kanjis = ''

        for item in data['data']:
            kanjis += item['data']['character']

        print('        /*{}{}:*/ "{}"{}'.format(' ' if level < 10 else '',
                                               level,
                                               kanjis,
                                               ',' if level < 60 else ''))


if __name__ == "__main__":
    main()
