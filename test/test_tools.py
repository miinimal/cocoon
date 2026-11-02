"""Offline tests for scripts/client-stake-check and scripts/testnet-backend.

Run: python3 test/test_tools.py   (needs pytoniq-core for the stake checker)
Fixtures are real client contract states read from TON mainnet.
"""
import json
import os
import socket
import subprocess
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(ROOT, 'test', 'fixtures')
failures = 0


def check(name, cond, detail=''):
    global failures
    print(('ok   ' if cond else 'FAIL ') + name + (f' -> {detail}' if detail and not cond else ''))
    failures += 0 if cond else 1


def stake_check(fixture):
    out = subprocess.run([sys.executable, os.path.join(ROOT, 'scripts', 'client-stake-check'), 'x', '--json',
                          '--from-file', os.path.join(FIX, fixture)], capture_output=True, text=True)
    return out.returncode, (json.loads(out.stdout) if out.returncode == 0 else out.stdout + out.stderr)


rc, info = stake_check('client-closed-stuck.json')
check('stuck client parsed', rc == 0, info)
if rc == 0:
    check('stuck client: state closed', info['state'] == 2)
    check('stuck client: stake 15 GRAM', info['stake'] == 15_000_000_000)
    check('stuck client: flagged STUCK', any(n.startswith('STUCK') for n in info['notes']), info['notes'])

rc, info = stake_check('client-open.json')
check('open client parsed', rc == 0, info)
if rc == 0:
    check('open client: state open', info['state'] == 0)
    check('open client: refund warning present', any('do not request a refund' in n for n in info['notes']), info['notes'])

# testnet-backend
s = socket.socket(); s.bind(('127.0.0.1', 0)); port = s.getsockname()[1]; s.close()
proc = subprocess.Popen([sys.executable, os.path.join(ROOT, 'scripts', 'testnet-backend'), '--port', str(port)],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    base = f'http://127.0.0.1:{port}'
    for _ in range(50):
        try:
            urllib.request.urlopen(base + '/health', timeout=1); break
        except Exception:
            time.sleep(0.1)

    def post(path, body):
        req = urllib.request.Request(base + path, data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
        return urllib.request.urlopen(req, timeout=5).read().decode()

    r = json.loads(post('/v1/chat/completions', {'model': 'm', 'messages': [{'role': 'user', 'content': 'hello'}]}))
    check('backend: chat reply', r['choices'][0]['message']['content'].endswith('hello'))
    check('backend: usage present', r['usage']['total_tokens'] == r['usage']['prompt_tokens'] + r['usage']['completion_tokens'])
    r = json.loads(post('/v1/chat/completions', {'model': 'm', 'messages': [{'role': 'user', 'content': 'w'}],
                                                  'tools': [{'type': 'function', 'function': {'name': 'get_weather'}}]}))
    check('backend: tool call', r['choices'][0]['finish_reason'] == 'tool_calls'
          and r['choices'][0]['message']['tool_calls'][0]['function']['name'] == 'get_weather')
    raw = post('/v1/chat/completions', {'model': 'm', 'stream': True, 'stream_options': {'include_usage': True},
                                        'messages': [{'role': 'user', 'content': 'stream me'}]})
    events = [json.loads(l[6:]) for l in raw.splitlines() if l.startswith('data: {')]
    check('backend: stream ends with [DONE]', raw.rstrip().endswith('data: [DONE]'))
    check('backend: stream usage chunk', events and 'usage' in events[-1])
    text = ''.join(e['choices'][0]['delta'].get('content', '') for e in events if e['choices'])
    check('backend: stream text reassembles', text.endswith('stream me'), text)
finally:
    proc.terminate()

print(f'{failures} test(s) FAILED' if failures else 'all tests passed')
sys.exit(1 if failures else 0)
