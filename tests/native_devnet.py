#!/usr/bin/env python3
"""Bridge actual C++ SDK RPC to a disposable native AploNode HTTP endpoint.
Run: python3 tests/run_native.py --devnet. APLO_TEST_RPC defaults to loopback 18745.
Only disposable devnet: unlocked synthetic faucet is required; random ephemeral owner/session keys.
"""
import json, os, pathlib, subprocess, sys, time, urllib.request
endpoint = os.environ.get('APLO_TEST_RPC', 'http://127.0.0.1:18745')
if not endpoint.startswith(('http://127.0.0.1:', 'http://localhost:')):
    raise SystemExit('Use an isolated loopback native devnet')
process = subprocess.Popen([sys.argv[1], '--integration'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
transcript = []
try:
    for line in process.stdout:
        req = json.loads(line)
        req.update(jsonrpc='2.0', id=len(transcript)+1)
        if req['method'] == 'eth_getTransactionReceipt':
            time.sleep(.25)
        request = urllib.request.Request(endpoint, json.dumps(req).encode(), {'Content-Type':'application/json'})
        with urllib.request.urlopen(request, timeout=10) as response:
            raw = response.read().decode()
        transcript.append({'request':req, 'response':json.loads(raw)})
        process.stdin.write(raw.replace('\n','')+'\n'); process.stdin.flush()
    code = process.wait(timeout=10)
finally:
    path = pathlib.Path(os.environ.get('APLO_TEST_TRANSCRIPT','/private/tmp/aplo-session-devnet-transcript.json'))
    path.write_text(json.dumps(transcript,indent=2)+'\n')
    print('RPC transcript:',path)
    if process.poll() is None: process.terminate()
if code: raise SystemExit(code)
