import os
import sys
import subprocess as sp


def execute(command, env, capture=True, input=None) -> bytes:
    passEnv = os.environ.copy()
    if env is not None:
        passEnv.update(env)
    if capture:
        return sp.check_output(command, env=passEnv, stderr=sys.stderr, input=input)
    else:
        return sp.check_call(
            command, env=passEnv, stdout=sys.stdout, stderr=sys.stderr, input=input
        )
