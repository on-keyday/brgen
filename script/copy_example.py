import shutil

shutil.copytree("./example", "./web/public/example", dirs_exist_ok=True)

# list copied files recursively and filter by suffix .bgn and
# write ./web/public/example/index.txt
import os
import pathlib

with open("./web/public/example/index.txt", "w") as f:
    for root, dirs, files in os.walk("./web/public/example"):
        for file in files:
            if file.endswith(".bgn"):
                path = pathlib.Path(os.path.join(root, file))
                path = path.relative_to("./web/public")
                f.write(path.as_posix() + "\n")
