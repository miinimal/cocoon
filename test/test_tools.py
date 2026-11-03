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
import urllib.error
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(ROOT, 'test', 'fixtures')
failures = 0


def check(name, cond, detail=''):
    global failures
    print(('ok   ' if cond else 'FAIL ') + name + (f' -> {detail}' if detail and not cond else ''))
    failures += 0 if cond else 1


def stake_check(fixture, *extra):
    out = subprocess.run([sys.executable, os.path.join(ROOT, 'scripts', 'client-stake-check'), 'x', '--json',
                          '--from-file', os.path.join(FIX, fixture), *extra], capture_output=True, text=True)
    return out.returncode, (json.loads(out.stdout) if out.returncode == 0 else out.stdout + out.stderr)


# real mainnet client: closed with stake 15; its proxy paid back 1.387108995 GRAM in total (checked 2026-11-02)
rc, info = stake_check('client-closed-stuck.json', '--paid-back-nano', '1387108995')
check('stuck client parsed', rc == 0, info)
if rc == 0:
    check('stuck client: state closed', info['state'] == 2)
    check('stuck client: stake 15 GRAM', info['stake'] == 15_000_000_000)
    check('stuck client: flagged STUCK', any(n.startswith('STUCK') for n in info['notes']), info['notes'])

rc, info = stake_check('client-closed-stuck.json', '--paid-back-nano', '15689000000')
check('refunded closed client: not STUCK', rc == 0 and not any(n.startswith('STUCK') for n in info['notes'])
      and any(n.startswith('Closed and refunded') for n in info['notes']), info)

rc, info = stake_check('client-open.json')
check('open client parsed', rc == 0, info)
if rc == 0:
    check('open client: state open', info['state'] == 0)
    check('open client: refund warning present', any('forced refund pays at most' in n for n in info['notes']), info['notes'])

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


# community-backend: relay to an OpenAI-compatible upstream (the stub stands in for Ollama / llama.cpp)
def free_port():
    s = socket.socket(); s.bind(('127.0.0.1', 0)); p = s.getsockname()[1]; s.close(); return p


def wait_up(url):
    for _ in range(50):
        try:
            urllib.request.urlopen(url, timeout=1); return True
        except Exception:
            time.sleep(0.1)
    return False


up_port, relay_port = free_port(), free_port()
upstream = subprocess.Popen([sys.executable, os.path.join(ROOT, 'scripts', 'testnet-backend'), '--port', str(up_port),
                             '--model', 'qwen3:8b'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
relay = subprocess.Popen([sys.executable, os.path.join(ROOT, 'scripts', 'community-backend'), '--port', str(relay_port),
                          '--model', 'Qwen/Qwen3-8B', '--upstream', f'http://127.0.0.1:{up_port}/v1',
                          '--upstream-model', 'qwen3:8b'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    wait_up(f'http://127.0.0.1:{up_port}/health')
    rb = f'http://127.0.0.1:{relay_port}'
    check('relay: health ok with live upstream', wait_up(rb + '/health'))

    def rpost(path, body):
        req = urllib.request.Request(rb + path, data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
        return urllib.request.urlopen(req, timeout=5).read().decode()

    models = json.loads(urllib.request.urlopen(rb + '/v1/models', timeout=5).read())
    check('relay: announces network model name', models['data'][0]['id'] == 'Qwen/Qwen3-8B')
    r = json.loads(rpost('/v1/chat/completions', {'model': 'Qwen/Qwen3-8B',
                                                   'messages': [{'role': 'user', 'content': 'relay hello'}]}))
    check('relay: chat passes through', r['choices'][0]['message']['content'].endswith('relay hello'))
    check('relay: model renamed back', r['model'] == 'Qwen/Qwen3-8B', r.get('model'))
    check('relay: usage present', r['usage']['total_tokens'] > 0)
    r = json.loads(rpost('/v1/chat/completions', {'model': 'Qwen/Qwen3-8B', 'messages': [{'role': 'user', 'content': 'w'}],
                                                   'tools': [{'type': 'function', 'function': {'name': 'lookup'}}]}))
    check('relay: tool calls pass through', r['choices'][0]['message']['tool_calls'][0]['function']['name'] == 'lookup')
    raw = rpost('/v1/chat/completions', {'model': 'Qwen/Qwen3-8B', 'stream': True,
                                         'stream_options': {'include_usage': True},
                                         'messages': [{'role': 'user', 'content': 'relay stream'}]})
    ev = [json.loads(l[6:]) for l in raw.splitlines() if l.startswith('data: {')]
    check('relay: stream ends with [DONE]', raw.rstrip().endswith('data: [DONE]'))
    check('relay: stream text reassembles', ''.join(e['choices'][0]['delta'].get('content', '')
                                                    for e in ev if e['choices']).endswith('relay stream'))
    check('relay: single usage chunk at the end', sum(1 for e in ev if e.get('usage')) == 1 and ev[-1].get('usage'))
    check('relay: stream model renamed', all(e['model'] == 'Qwen/Qwen3-8B' for e in ev))
    raw = rpost('/v1/chat/completions', {'model': 'Qwen/Qwen3-8B', 'stream': True,
                                         'messages': [{'role': 'user', 'content': 'no usage asked'}]})
    check('relay: no usage chunk when not asked', not any(json.loads(l[6:]).get('usage')
                                                         for l in raw.splitlines() if l.startswith('data: {')))
    try:
        rpost('/v1/chat/completions', {'x': 'y' * (5 * 1024 * 1024)})
        check('relay: oversized body refused', False)
    except urllib.error.HTTPError as e:
        check('relay: oversized body refused', e.code == 413, e.code)
    except Exception:  # connection reset after early reply is also a refusal
        check('relay: oversized body refused', True)
finally:
    upstream.terminate()

try:  # upstream gone -> clear 502 / 503, never a hang
    time.sleep(0.3)
    try:
        urllib.request.urlopen(rb + '/health', timeout=5)
        check('relay: health reports upstream down', False)
    except urllib.error.HTTPError as e:
        check('relay: health reports upstream down', e.code == 503, e.code)
    try:
        rpost('/v1/chat/completions', {'messages': [{'role': 'user', 'content': 'x'}]})
        check('relay: 502 when upstream down', False)
    except urllib.error.HTTPError as e:
        check('relay: 502 when upstream down', e.code == 502, e.code)
finally:
    relay.terminate()

print(f'{failures} test(s) FAILED' if failures else 'all tests passed')
sys.exit(1 if failures else 0)
