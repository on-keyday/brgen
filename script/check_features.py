#!/usr/bin/env python3
"""spec/features.json の機械的な整合だけを検査する。

台帳が腐る形は 2 つある。

1. 台帳に載っているが実体が無い  -> covers のパスが存在しない
2. 実体があるが台帳に載っていない -> feature_test の .bgn がどの feature からも
   参照されていない

どちらもパス集合の差で出るので、人の判断を要しない。逆に「この機能は本当に
実装されているか」「どのバックエンドが対応しているか」はここでは見ない。
それは測定の仕事で、台帳に書くものではない (軸5 の分業)。

    python script/check_features.py          # 検査
    python script/check_features.py --list    # 台帳を一覧表示
"""

import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join(ROOT, "spec", "features.json")
# 台帳が覆っているべき入力。ここ以外の example/*.bgn は実フォーマットであって
# 機能の見本ではないので、対象にしない。
COVERED_DIR = os.path.join(ROOT, "example", "feature_test")

ID_RE = re.compile(r"^F[0-9]{4}$")
SLUG_RE = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)*$")


def load():
    with open(LEDGER, encoding="utf-8") as f:
        return json.load(f)


REQUIRED = ("id", "slug", "name", "category", "summary", "covers")
OPTIONAL = ("notes",)
CATEGORIES = ("lexical", "declaration", "type", "field", "control-flow",
              "expression", "stream", "analysis", "meta")


def check_shape(features):
    """brgen_features_schema.json の形を、jsonschema 無しで確かめる。

    台帳は 1 つしかないので、検査のためだけに依存を増やす価値がない。
    schema 側を直したらこちらも直すこと。
    """
    problems = []
    for i, f in enumerate(features):
        where = f.get("id") or "features[{}]".format(i)
        for key in REQUIRED:
            if key not in f:
                problems.append("{}: missing required key '{}'".format(where, key))
        for key in f:
            if key not in REQUIRED and key not in OPTIONAL:
                problems.append("{}: unknown key '{}'".format(where, key))
        if f.get("category") not in CATEGORIES:
            problems.append("{}: category '{}' is not one of {}".format(
                where, f.get("category"), ", ".join(CATEGORIES)))
        if not isinstance(f.get("covers"), list):
            problems.append("{}: covers must be an array".format(where))
    return problems


def check(ledger):
    features = ledger["features"]
    problems = check_shape(features)
    if problems:
        # 形が壊れていると以降の検査が例外になるので、ここで返す
        return problems

    seen_id, seen_slug = {}, {}
    for f in features:
        fid, slug = f["id"], f["slug"]
        if not ID_RE.match(fid):
            problems.append("{}: id is not F<4 digits>".format(fid))
        if not SLUG_RE.match(slug):
            problems.append("{}: slug is not kebab-case: {}".format(fid, slug))
        if fid in seen_id:
            problems.append("{}: duplicate id (also {})".format(fid, seen_id[fid]))
        if slug in seen_slug:
            problems.append("{}: duplicate slug '{}' (also {})".format(fid, slug, seen_slug[slug]))
        seen_id[fid], seen_slug[slug] = slug, fid

    # 1. 台帳が指す入力が実在するか
    referenced = set()
    for f in features:
        for rel in f["covers"]:
            path = os.path.join(ROOT, rel.replace("/", os.sep))
            if not os.path.exists(path):
                problems.append("{}: covers a path that does not exist: {}".format(f["id"], rel))
            else:
                referenced.add(os.path.normcase(os.path.abspath(path)))

    # 2. 入力が台帳から参照されているか
    for path in sorted(glob.glob(os.path.join(COVERED_DIR, "*.bgn"))):
        if os.path.normcase(os.path.abspath(path)) not in referenced:
            problems.append("not covered by any feature: {}".format(
                os.path.relpath(path, ROOT).replace(os.sep, "/")))

    return problems


def show(ledger):
    by_category = {}
    for f in ledger["features"]:
        by_category.setdefault(f["category"], []).append(f)
    for category in sorted(by_category):
        print("[{}]".format(category))
        for f in sorted(by_category[category], key=lambda x: x["id"]):
            mark = "" if f["covers"] else "   (no input)"
            print("  {}-{}{}".format(f["id"], f["slug"], mark))
        print()


def main():
    parser = argparse.ArgumentParser(description="check spec/features.json")
    parser.add_argument("--list", action="store_true", help="print the ledger instead of checking")
    args = parser.parse_args()

    ledger = load()
    if args.list:
        show(ledger)
        return 0

    problems = check(ledger)
    for p in problems:
        print("error: " + p)
    n = len(ledger["features"])
    no_input = sum(1 for f in ledger["features"] if not f["covers"])
    print("{} features, {} without a dedicated input, {} problem(s)".format(n, no_input, len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
