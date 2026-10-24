#!/usr/bin/env python3
"""Opt-in ESP regression: loading HTML must not stall the following API call.

Run with no serial monitor attached. This performs read-only HTTP requests and
is intentionally excluded from the offline suite. Example:
  python3 tests/hardware_http_latency.py http://192.0.2.10 --max-gap-ms 1500
"""
import argparse
import gzip
import json
import statistics
import time
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('url', help='ESP HTTP origin')
parser.add_argument('--rounds', type=int, default=5, choices=range(1, 21))
parser.add_argument('--max-gap-ms', type=float, default=1500)
args = parser.parse_args()
origin = args.url.rstrip('/')
rows = []
for _ in range(args.rounds):
    with urllib.request.urlopen(origin + '/', timeout=8) as response:
        document = response.read()
        if response.headers.get('Content-Encoding') == 'gzip':
            document = gzip.decompress(document)
        assert b'pc_blind_mode' in document, 'Unexpected dashboard document'
    started = time.monotonic()
    with urllib.request.urlopen(origin + '/api/v2/setup', timeout=8) as response:
        assert 'required' in json.load(response), 'Unexpected setup response'
    rows.append((time.monotonic() - started) * 1000)
median = statistics.median(rows)
print(json.dumps({'after_html_ms': rows, 'median_ms': median}))
if median >= args.max_gap_ms:
    raise SystemExit('FAIL: repeatable pause after HTML transfer')
print('PASS: no serial-monitor-dependent stall after the dashboard response')
