import pathlib as pl
import shutil as sh

if not pl.Path("./web/dev/src/lib/bmgen").exists():
    sh.copytree("./web/dev/stub/bmgen", "./web/dev/src/lib/bmgen")
