"""nast2go を example/ 全体に回して、go vet を通るかを数える。

    python src/core/nast/backends/go/measure.py [-j 4] [--nast2go PATH] [--out DIR]

1 入力ごとに 1 パッケージ (DIR/<名前>/gen.go) を作り、DIR に go.mod を 1 つ
置いて `go vet ./...` を 1 回だけ回す (パッケージごとに go を起動すると遅い)。

数えるもの:
  - format: 生成した / 飛ばした (理由ごと)
  - 入力: 全 format を生成し go vet を通った / 飛ばした format がある / vet で落ちた
"""
import argparse
import collections
import concurrent.futures
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[5]


def package_name(path: pathlib.Path) -> str:
    rel = path.relative_to(ROOT / "example").with_suffix("")
    name = re.sub(r"[^0-9A-Za-z_]", "_", "_".join(rel.parts)).lower()
    if name[0].isdigit():
        name = "p" + name
    return name


def generate(nast2go: pathlib.Path, src: pathlib.Path, out: pathlib.Path):
    pkg = package_name(src)
    d = out / pkg
    d.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(
        [str(nast2go), "-i", str(src), "-o", str(d / "gen.go"), "--package", pkg],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
    )
    skipped = [l for l in r.stderr.splitlines() if "skipped " in l]
    generated = (d / "gen.go").exists() and r.returncode == 0
    if not generated:
        shutil.rmtree(d, ignore_errors=True)
    return pkg, src, generated, skipped, r.stderr if not generated else ""


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=4)
    ap.add_argument("--nast2go", default=str(ROOT / "src/core/nast/build/fast/bin/nast2go.exe"))
    ap.add_argument("--out", default=str(ROOT / "ignore/nast2go/corpus"))
    ap.add_argument("--detail", action="store_true", help="vet の失敗を全部出す")
    args = ap.parse_args()

    out = pathlib.Path(args.out)
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    (out / "go.mod").write_text("module corpus\n\ngo 1.22\n", encoding="utf-8")

    sources = sorted((ROOT / "example").rglob("*.bgn"))
    results = []
    with concurrent.futures.ThreadPoolExecutor(args.j) as ex:
        for r in ex.map(lambda s: generate(pathlib.Path(args.nast2go), s, out), sources):
            results.append(r)

    vet = subprocess.run(["go", "vet", "./..."], cwd=out, capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    failed = collections.defaultdict(list)
    current = None
    for line in vet.stderr.splitlines():
        m = re.match(r"^# corpus/(\S+)", line)
        if m:
            current = m.group(1)
            continue
        m = re.match(r"^(?:vet: )?\.?[\\/]?(\w+)[\\/]gen\.go:\d+:\d+: (.*)", line)
        if m:
            failed[m.group(1)].append(m.group(2))
            continue
        if current and line.strip():
            failed[current].append(line.strip())

    fmt_ok = 0
    reasons = collections.Counter()
    status = collections.Counter()
    vet_reasons = collections.Counter()
    for pkg, src, generated, skipped, err in results:
        for s in skipped:
            m = re.search(r"\d+:\d+: (.*)$", s)
            reasons[m.group(1) if m else s] += 1
        if not generated:
            status["生成できない (入力の解析で失敗など)"] += 1
            continue
        if pkg in failed:
            status["go vet で落ちた"] += 1
            for msg in failed[pkg][:1]:
                vet_reasons[re.sub(r"\b\w+\d+\b", "X", msg)[:90]] += 1
            if args.detail:
                print(f"== {src.relative_to(ROOT)}")
                for msg in failed[pkg][:5]:
                    print("   ", msg)
        elif skipped:
            status["vet は通るが飛ばした format がある"] += 1
        else:
            status["全 format を生成し go vet を通った"] += 1

    gen_text = "".join((out / p / "gen.go").read_text(encoding="utf-8") for p, _, g, _, _ in results if g)
    fmt_ok = gen_text.count(") DecodeSlice(buf []byte")

    print(f"入力 {len(results)} 本")
    for k, v in status.most_common():
        print(f"  {v:4}  {k}")
    print(f"format: 生成 {fmt_ok} / 飛ばした {sum(reasons.values())}")
    for k, v in reasons.most_common(30):
        print(f"  {v:4}  {k}")
    if vet_reasons:
        print("go vet の最初の失敗:")
        for k, v in vet_reasons.most_common(20):
            print(f"  {v:4}  {k}")


if __name__ == "__main__":
    main()
