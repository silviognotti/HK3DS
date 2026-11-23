"""Helpers for cancelling thread/process workers cleanly on interruption."""

from contextlib import contextmanager
import sys


@contextmanager
def parallel_executor(factory, **kwargs):
    executor = factory(**kwargs)
    try:
        yield executor
    except KeyboardInterrupt:
        executor.shutdown(wait=False, cancel_futures=True)
        print("\nInterrupted: cancelling worker tasks.", file=sys.stderr)
        raise
    except BaseException:
        executor.shutdown(wait=False, cancel_futures=True)
        raise
    else:
        executor.shutdown(wait=True)


def interruptible_main(main, *args, **kwargs):
    try:
        result = main(*args, **kwargs)
        return result if isinstance(result, int) else 0
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        return 130
