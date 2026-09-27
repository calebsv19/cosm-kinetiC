#!/usr/bin/env python3
"""PhysicsSim trusted-local CLI / MCP stdio transport. stdout is protocol-only."""
import argparse
import json
from pathlib import Path
import sys

# Packaged scripts are sealed resources; runtime caches must not mutate the app.
sys.dont_write_bytecode = True

sys.path.insert(0, str(Path(__file__).resolve().parent / 'agent_session'))
from service import Service, SessionError, encode
from protocol import call, tools
from preview import image_content
import subprocess


def serve(service):
    initialized = False
    for line in sys.stdin:
        request = None
        try:
            request = json.loads(line)
            if not isinstance(request,dict) or request.get('jsonrpc') != '2.0' or not isinstance(request.get('method'),str):
                raise ValueError('invalid JSON-RPC request')
            method = request['method']
            if 'id' not in request:
                continue
            if method == 'initialize':
                initialized = True
                proposed = request.get('params',{}).get('protocolVersion')
                version = proposed if proposed in ('2024-11-05','2025-03-26','2025-06-18','2025-11-25') else '2025-11-25'
                result = {'protocolVersion':version,'capabilities':{'tools':{'listChanged':False}},
                          'serverInfo':{'name':'physics-sim-local','version':'0.1.0'},
                          'instructions':'Trusted-local sessions. Discover capabilities, create and validate a Wind template, then start a run. Pause/step receipts are authoritative. Wind is approximate, not validated CFD.'}
            elif method == 'ping':
                result = {}
            elif not initialized:
                raise ValueError('initialize first')
            elif method == 'tools/list':
                result = {'tools':tools()}
            elif method == 'tools/call':
                params = request.get('params',{})
                try:
                    value = call(service,params.get('name'),params.get('arguments',{}))
                    image = image_content(value) if params.get('name') in ('run_inspect','run_sample') else None
                    if image:
                        value['preview'].pop('samples', None)
                        value['preview']['display'] = 'field heatmap; white solids; range and vector scale in metadata'
                    content = [{'type':'text','text':encode(value)}]
                    if image:
                        content.append(image)
                    result = {'content':content, 'structuredContent':value, 'isError':False}
                except (SessionError, OSError, ValueError, TypeError, TimeoutError, subprocess.SubprocessError) as exc:
                    result = {'content':[{'type':'text','text':str(exc)}], 'isError':True}
            else:
                print(encode({'jsonrpc':'2.0','id':request['id'],'error':{'code':-32601,'message':'Method not found'}}),flush=True)
                continue
            response = {'jsonrpc':'2.0','id':request['id'],'result':result}
        except (ValueError, TypeError) as exc:
            response = {'jsonrpc':'2.0','id':request.get('id') if isinstance(request,dict) else None,
                        'error':{'code':-32700 if request is None else -32600,'message':str(exc)}}
        print(encode(response),flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root')
    parser.add_argument('--worker')
    parser.add_argument('--mcp',action='store_true')
    parser.add_argument('--output',help='Atomically save the CLI response for an asynchronous desktop client')
    parser.add_argument('operation',nargs='?')
    parser.add_argument('arguments',nargs='?',default='{}')
    args = parser.parse_args()
    service = Service(args.root,args.worker)
    if args.mcp:
        serve(service); return 0
    try:
        value = call(service,args.operation,json.loads(args.arguments))
        code = 0
    except (SessionError,OSError,ValueError,TypeError,TimeoutError,subprocess.SubprocessError) as exc:
        value = {'error':str(exc)}; code = 1
    if args.output:
        from service import atomic
        atomic(args.output,value)
    else:
        print(encode(value))
    return code


if __name__ == '__main__':
    sys.exit(main())
