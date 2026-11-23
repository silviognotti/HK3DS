import argparse
import glob
import os
import pickle
import struct
from concurrent.futures import ProcessPoolExecutor, as_completed
from parallel_helpers import interruptible_main, parallel_executor

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
ROOT = os.environ.get("HKDATA", "hkdata")


def process_texture(path):
    with open(path, "rb") as handle:
        header = handle.read(24)
    width, height = struct.unpack(">II", header[16:24])
    if width * height < 256 * 256:
        return None
    try:
        image = Image.open(path)
        image.draft(None, None)
        alpha = np.asarray(
            image.getchannel("A") if "A" in image.getbands() else image.convert("L")
        )
        return path, (width, height, alpha[::4, ::4] > 8)
    except Exception as error:
        print("err", path, error, flush=True)
        return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workers", type=int, default=min(16, os.cpu_count() or 1))
    args = parser.parse_args()
    paths = sorted(glob.glob(f"{ROOT}/hs/Texture2D/*.png"))
    out = {}
    with parallel_executor(ProcessPoolExecutor, max_workers=max(1, args.workers)) as pool:
        futures = {pool.submit(process_texture, path): path for path in paths}
        for index, future in enumerate(as_completed(futures), 1):
            result = future.result()
            if result:
                path, value = result
                out[path] = value
            if index % 100 == 0:
                print(index, flush=True)
    with open(f"{ROOT}/hs_alpha4.pkl", "wb") as handle:
        pickle.dump(out, handle)
    print("done", len(out))


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main))
