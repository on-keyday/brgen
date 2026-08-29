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

// ConstantValue (Evaluator が畳んだ定数) の表示。
function constLabel(v: nast.ConstantValue): string {
    if (v.kind === "integer") {
        return `${v.is_negative ? "-" : ""}${v.integer}`;
    }
    if (v.kind === "boolean") {
        return v.boolean ? "true" : "false";
    }
    // 文字列は base64 (binary_value のまま)。表示のときだけ復号する。
    try {
        return JSON.stringify(Buffer.from(v.string, "base64").toString("latin1"));
    } catch {
        return JSON.stringify(v.string);
    }
}

function constSuffix(tables: nast.SideTables, e: nast.NodeId): string {
    const cv = tables.get<nast.ConstantValue>("ConstantValue", e);
    return cv !== undefined ? ` = ${constLabel(cv)}` : "";
}

// 式の種別に添える中身。Binary / Unary はどの演算かを記号で出す。
function exprDetail(kind: nast.NodeKind, d: any): string {
    if (kind === "Binary") {
        const op = d.op as nast.BinaryOp;
        return ` \`${nast.ENUM_DISPLAY.BinaryOp[op] ?? op}\``;
    }
    if (kind === "Unary") {
        const op = d.op as nast.UnaryOp;
        return ` \`${nast.ENUM_DISPLAY.UnaryOp[op] ?? op}\``;
    }
    return "";
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

// 定義側の ident (フィールド名や format 名) の表示。参照と違って Resolution が
// 無いので、覆っている最小の文が自分を name に持つときに宣言として出す。
// 型は木に置かれているものだけを見せる (ここで型付けをやり直さない):
//   Field / Parameter    宣言に書かれた型
//   Format / State       参照されたときに作られる struct_type (無ければ種別のみ)
//   Enum / EnumMember    enum_type
//   Function             引数と戻り値から組んだ表示
//   VariableDefinition   右辺の式に付いた型。for x in c の束縛も同じノード
//                        (op=in_assign) で、束縛自体の型は木に無いので
//                        container の型を添えて出す
function declLine(a: nast.Arena, tables: nast.SideTables, ident: nast.NodeId, stmt: nast.NodeId): string | null {
    const kind = a.kind(stmt);
    const d = a.data<any>(stmt);
    if (kind === null || d === null) {
        return null;
    }
    const declares = typeof d.name === "number" && d.name === ident;
    if (!declares) {
        return null;
    }
    const name = identText(a, ident);
    let value = "";
    let label: string | null = null;
    if (kind === "Enum") {
        label = typeLabel(a, (d as nast.Enum).enum_type);
    }
    else if (kind === "EnumMember") {
        const en = a.data<nast.Enum>((d as nast.EnumMember).belong);
        label = en !== null ? typeLabel(a, en.enum_type) : null;
        value = constSuffix(tables, (d as nast.EnumMember).value);
    }
    else if (kind === "Function") {
        const fn = d as nast.Function;
        const params = fn.parameters
            .map(p => typeLabel(a, a.data<nast.Parameter>(p)?.type ?? undefined))
            .join(", ");
        const ret = nast.isNullId(fn.return_type) ? "" : ` -> ${typeLabel(a, fn.return_type)}`;
        label = `fn(${params})${ret}`;
    }
    else if (kind === "VariableDefinition") {
        const vd = d as nast.VariableDefinition;
        const v = a.data<nast.Expr>(vd.value);
        if (vd.op === "in_assign") {
            const container = v !== null ? typeLabel(a, v.type) : null;
            return container !== null
                ? `for \`${name}\` in \`${container}\` (definition)`
                : `for \`${name}\` (definition)`;
        }
        label = v !== null ? typeLabel(a, v.type) : null;
        value = constSuffix(tables, vd.value);
    }
    else if (a.is(stmt, "NamedStructTypedStatement")) {
        const st = (d as nast.NamedStructTypedStatement).struct_type;
        label = nast.isNullId(st) ? null : typeLabel(a, st);
    }
    else if (a.is(stmt, "NamedTypeStatement")) {
        label = typeLabel(a, (d as nast.NamedTypeStatement).type);
    }
    if (label === name) {
        // format 名の型はその format 自身なので、二度言わない。
        label = null;
    }
    const head = label !== null ? `\`${label}\` — ${kind}` : kind;
    return `${head} \`${name}\` (definition)${value}${requirementsLine(a, tables, stmt)}`;
}

// requires 推論 (Requirements 表) の中身。Format / Function の定義に添える。
// decode / encode の 2 組を持つ。同じなら方向を出さずに 1 行で言う。
function requirementsLine(a: nast.Arena, tables: nast.SideTables, stmt: nast.NodeId): string {
    const req = tables.get<nast.Requirements>("Requirements", stmt);
    if (req === undefined) {
        return "";
    }
    const names = (ids: nast.NodeId[]) =>
        ids.map(s => identText(a, a.data<nast.StateVariable>(s)!.name)).join(", ");
    const side = (prefix: "decode" | "encode"): string[] => {
        const parts: string[] = [];
        const caps = (["peek", "backward", "remain", "offset"] as const)
            .filter(c => req[`${prefix}_${c}`]);
        if (caps.length > 0) {
            parts.push(`input: ${caps.join(", ")}`);
        }
        const reads = req[`${prefix}_state_read`];
        const writes = req[`${prefix}_state_write`];
        if (reads.length > 0) {
            parts.push(`reads: ${names(reads)}`);
        }
        if (writes.length > 0) {
            parts.push(`writes: ${names(writes)}`);
        }
        return parts;
    };
    const dec = side("decode");
    const enc = side("encode");
    if (dec.length === 0 && enc.length === 0) {
        return "\n\nrequires: nothing";
    }
    if (dec.join(";") === enc.join(";")) {
        return `\n\nrequires — ${dec.join("; ")}`;
    }
    const lines: string[] = [];
    lines.push(`decode: ${dec.length > 0 ? dec.join("; ") : "nothing"}`);
    lines.push(`encode: ${enc.length > 0 ? enc.join("; ") : "nothing"}`);
    return `\n\nrequires — ${lines.join(" / ")}`;
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
    // assert (文の位置の真偽式) と error(...) は文で、式の行だけだと存在が
    // 見えないので別枠で拾う。assert は loc が条件式と同じ。
    let bestGuard: nast.NodeId | null = null;
    let wIdent = Infinity, wExpr = Infinity, wType = Infinity, wGuard = Infinity;
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
        if ((h.type === "Assert" || h.type === "ExplicitError") && w <= wGuard) {
            bestGuard = id;
            wGuard = w;
        }
    }

    const lines: string[] = [];
    let rangeOf: nast.NodeId | null = null;
    let exprLinePushed = false;
    if (bestExpr !== null && (bestType === null || wExpr <= wType)) {
        const kind = a.kind(bestExpr)!;
        const expr = a.data<nast.Expr>(bestExpr)!;
        lines.push(`\`${typeLabel(a, expr.type)}\` — ${kind}${exprDetail(kind, expr)}${constSuffix(dump.tables, bestExpr)}`);
        rangeOf = bestExpr;
        exprLinePushed = true;
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
        else {
            // 定義側。宣言文の loc は名前を覆わないことがある (format A: の
            // loc は `format` まで) ので、覆いではなく name の一致で逆引きする。
            for (const id of a.ids()) {
                if (!reach.has(nast.idIndex(id)) || !a.is(id, "Statement")) {
                    continue;
                }
                const line = declLine(a, dump.tables, bestIdent, id);
                if (line !== null) {
                    // enum メンバの合成値のような、名前と同じ位置に置かれた式の
                    // 行は定義の表示とかぶるので落とす。
                    if (exprLinePushed && wExpr >= wIdent) {
                        lines.shift();
                    }
                    lines.push(line);
                    rangeOf = bestIdent;
                    break;
                }
            }
        }
    }
    if (bestGuard !== null) {
        const gk = a.kind(bestGuard)!;
        if (gk === "Assert") {
            // 条件が定数に畳めているなら結果も見せる。常に false の assert は
            // それ自体が発見。
            const st = a.data<nast.Assert>(bestGuard)!;
            lines.push(`Assert${constSuffix(dump.tables, st.expr)}`);
        }
        else {
            const st = a.data<nast.ExplicitError>(bestGuard)!;
            lines.push(`ExplicitError${constSuffix(dump.tables, st.message)}`);
        }
        rangeOf ??= bestGuard;
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
