/*license*/
// nast_dump の出力 (nast_nodes.ts が読む形) から hover の中身を作る。
// ast2ts に対する analyze.ts の nast 版。型の表示は src/core/nast の Typer が
// 式に載せた結果をそのまま見せるだけで、ここでは型付けをやり直さない。
import * as nast from "./nast_nodes";

// 型を 1 行の短い表示にする。構文の復元ではなく近似 (配列長の式など)。
export function typeLabel(a: nast.Arena, t: nast.NodeId | undefined, depth = 0): string {
    if (t === undefined || nast.isNullId(t)) {
        return "(no type)";
    }
    if (depth > 8) {
        return "...";
    }
    const kind = a.kind(t);
    const d = a.data<any>(t);
    if (kind === null || d === null) {
        return "(no type)";
    }
    switch (kind) {
        case "IntType": {
            const it = d as nast.IntType;
            const endian = it.endian === "big" ? "b" : it.endian === "little" ? "l" : "";
            return (it.is_signed ? "i" : "u") + endian + String(it.bit_size);
        }
        case "FloatType": {
            const ft = d as nast.FloatType;
            const endian = ft.endian === "big" ? "b" : ft.endian === "little" ? "l" : "";
            return "f" + endian + String(ft.bit_size);
        }
        case "BoolType":
            return "bool";
        case "VoidType":
            return "void";
        case "MetaType":
            return "type";
        case "IntLiteralType": {
            const lit = a.data<nast.IntLiteral>((d as nast.IntLiteralType).base);
            return lit !== null ? `int literal ${lit.value}` : "int literal";
        }
        case "StrLiteralType":
            return "string literal";
        case "RegexLiteralType":
            return "regex literal";
        case "ArrayType": {
            const at = d as nast.ArrayType;
            return `[${exprLabelShort(a, at.length)}]${typeLabel(a, at.element_type, depth + 1)}`;
        }
        case "FunctionType": {
            const ft = d as nast.FunctionType;
            const params = ft.parameters.map(p => typeLabel(a, p, depth + 1)).join(", ");
            const ret = nast.isNullId(ft.return_type) ? "" : ` -> ${typeLabel(a, ft.return_type, depth + 1)}`;
            return `fn(${params})${ret}`;
        }
        case "StructType": {
            const owner = (d as nast.StructType).base;
            if (a.is(owner, "NamedStatement")) {
                return identText(a, a.data<nast.NamedStatement>(owner)!.name);
            }
            return a.kind(owner) === "Module" ? "module" : "struct";
        }
        case "InlineStructType":
            return "struct (inline)";
        case "StructUnionType":
            return "struct union";
        case "UnionType": {
            const ut = d as nast.UnionType;
            if (!nast.isNullId(ut.common_type)) {
                return `union -> ${typeLabel(a, ut.common_type, depth + 1)}`;
            }
            return "union";
        }
        case "EnumType": {
            const en = a.data<nast.Enum>((d as nast.EnumType).base);
            return en !== null ? identText(a, en.name) : "enum";
        }
        case "RangeType":
            return `range<${typeLabel(a, (d as nast.RangeType).base_type, depth + 1)}>`;
        case "OptionalType":
            return typeLabel(a, (d as nast.OptionalType).base_type, depth + 1) + "?";
        case "GenericType":
            return typeLabel(a, (d as nast.GenericType).base_type, depth + 1) + "<...>";
        case "StreamType": {
            const st = d as nast.StreamType;
            const base = st.kind === "input_" ? "input stream" : st.kind === "output_" ? "output stream" : "stream";
            return nast.isNullId(st.length) ? base : `${base} (length = ${exprLabelShort(a, st.length)})`;
        }
        case "IdentType": {
            const it = d as nast.IdentType;
            const name = identText(a, it.ident);
            if (!nast.isNullId(it.base)) {
                const base = typeLabel(a, it.base, depth + 1);
                if (base !== name) {
                    return `${name} (= ${base})`;
                }
            }
            return name;
        }
        default:
            // ImportedType など、base を持つ包み全般。
            if (nast.isDerived(kind, "WrapperType")) {
                const w = d as nast.WrapperType;
                if (!nast.isNullId(w.base)) {
                    return typeLabel(a, w.base, depth + 1);
                }
            }
            return kind;
    }
}

function identText(a: nast.Arena, id: nast.NodeId): string {
    return a.data<nast.Ident>(id)?.identifier ?? "?";
}

// 配列長などに現れる式の近似表示。リテラルと参照だけ実体を出す。
function exprLabelShort(a: nast.Arena, e: nast.NodeId): string {
    if (nast.isNullId(e)) {
        return "..";
    }
    const kind = a.kind(e);
    if (kind === "IntLiteral") {
        return a.data<nast.IntLiteral>(e)!.value;
    }
    if (kind === "Reference") {
        return identText(a, a.data<nast.Reference>(e)!.name);
    }
    return "expr";
}

export interface NastHoverResult {
    markdown: string;
    begin: number;
    end: number;
}

// 位置を覆う一番小さいノードを、木から到達できるものの中から選ぶ。
// アリーナ全体を見るとパーサが捨てた孤児 (型なし) に当たるので reach で絞る。
export function hoverAt(dump: nast.Dump, reach: Set<number>, pos: number): NastHoverResult | null {
    const a = dump.arena;
    if (a === null) {
        return null;
    }
    let bestIdent: nast.NodeId | null = null;
    let bestExpr: nast.NodeId | null = null;
    let bestType: nast.NodeId | null = null;
    let wIdent = Infinity, wExpr = Infinity, wType = Infinity;
    for (const id of a.ids()) {
        if (!reach.has(nast.idIndex(id))) {
            continue;
        }
        const h = a.header(id)!;
        if (h.loc.file !== dump.mainFile) {
            continue;
        }
        const b = h.loc.pos.begin, e = h.loc.pos.end;
        if (!(b <= pos && pos <= e)) {
            continue;
        }
        const w = e - b;
        if (h.type === "Ident" && w <= wIdent) {
            bestIdent = id;
            wIdent = w;
        }
        if (nast.isDerived(h.type, "Expr") && w <= wExpr) {
            bestExpr = id;
            wExpr = w;
        }
        if (nast.isDerived(h.type, "Type") && w <= wType) {
            bestType = id;
            wType = w;
        }
    }

    const lines: string[] = [];
    let rangeOf: nast.NodeId | null = null;
    if (bestExpr !== null && (bestType === null || wExpr <= wType)) {
        const kind = a.kind(bestExpr)!;
        const expr = a.data<nast.Expr>(bestExpr)!;
        lines.push(`\`${typeLabel(a, expr.type)}\` — ${kind}`);
        rangeOf = bestExpr;
    }
    else if (bestType !== null) {
        // 型の位置に書かれたもの (u8 や format 名) はその型自身を出す。
        lines.push(`\`${typeLabel(a, bestType)}\` — ${a.kind(bestType)!}`);
        rangeOf = bestType;
    }
    if (bestIdent !== null) {
        const res = dump.tables.get<nast.Resolution>("Resolution", bestIdent);
        if (res !== undefined && !nast.isNullId(res.target)) {
            const targetKind = a.kind(res.target) ?? "?";
            let name = "";
            if (a.is(res.target, "NamedStatement")) {
                name = ` \`${identText(a, a.data<nast.NamedStatement>(res.target)!.name)}\``;
            }
            lines.push(`→ ${targetKind}${name}`);
            rangeOf ??= bestIdent;
        }
    }
    if (lines.length === 0 || rangeOf === null) {
        return null;
    }
    const loc = a.loc(rangeOf)!;
    return { markdown: lines.join("\n\n"), begin: loc.pos.begin, end: loc.pos.end };
}

// 到達集合。dump 1 回につき 1 回作って使い回す。
export function reachOf(dump: nast.Dump): Set<number> {
    if (dump.arena === null) {
        return new Set();
    }
    const roots = dump.root !== null ? [dump.root, ...dump.modules] : dump.modules;
    return nast.reachableFrom(dump.arena, roots);
}
