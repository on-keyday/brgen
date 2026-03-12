import os
import pathlib as pl
import json


def collect_langs(root_dir):
    cmake_files = []
    for root, dirs, files in os.walk(root_dir):
        for d in dirs:
            if d != "bm2" and str(d).startswith("bm2"):
                if d == "bm2hexmap":
                    continue
                cmake_files.append(
                    {
                        "name": d[3:],
                        "can_generate": pl.Path(
                            os.path.join(root, d, "CMakeLists.txt")
                        ).exists(),
                    }
                )
    return cmake_files


print(json.dumps(collect_langs("src")))
