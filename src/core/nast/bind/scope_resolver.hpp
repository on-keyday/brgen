/*license*/
#pragma once
#include "../nodes.h"

#include <core/common/error.h>
#include <string>
#include <string_view>
#include <vector>

// 名前を解決して Resolution 表に書く。
//
// 規則は rebrgen/docs/draft/scope_rules.md にまとめたものを再現する:
//   - format / state / enum は前方参照でき、外側のスコープからも見える
//   - field / 変数 / 定数 / fn は後方参照のみで、format / state / enum の境界を越えると見えない
//   - どちらでも見つからなければ global を前方も含めて舐める
//
// 元の実装 (src/core/ast/node/scope.h) と違い、Scope グラフをノードに持たせない。
// 木と文の順序とノード種から毎回組み直す。区間 (Scope::next) の連なりは
// 宣言 1 つあたりの position 番号に畳んである。
//
// example/ 309 ファイルで src2json と突き合わせた結果 (2026-08-11): 一致 6265 /
// 解決先が違う 62 / src2json だけ解決 0 / nast だけ解決 19。
// **src2json が解決して nast が解決しない参照は無い。**
// 「解決先が違う」62 件は全部 for x in ... の x で、nast が束縛の
// VariableDefinition (RangeLoop::binding) を、src2json が束縛の Ident を指す
// 表現差 (62/62 で行が一致)。
//
// 分岐で宣言された名前は binder が合成した Field (UnionFields 表) を通して見える。
// import した先の名前はここでは見ない。ImportResolution は ImportResolver が
// 埋めるが、それを引くのはメンバアクセス (dns.Type) の解決で、元の実装でも
// 型解析側の仕事 (typing.cpp:1188)。この段は 1 つの Module の中しか見ない。

namespace brgen::nast::bind {

    // 宣言 1 つ。
    struct Decl {
        std::string name;
        Node<Statement> node;
        // ブロック内での位置。文 i は i+1、fn の引数のように本体より前に入るものは 0。
        std::size_t position = 0;
        // 前方参照でき、外側からも見える種類か。元の is_type_ident に当たる。
        bool is_type = false;
    };

    // ブロック 1 つ分の宣言。
    struct Env {
        Env* parent = nullptr;
        // 親での自分の位置。外へ出るときに後方参照の判定へ引き継ぐ。
        std::size_t position_in_parent = 0;
        // format / state / enum の本体。ここを越えて外を見るときは型だけになる。
        bool type_barrier = false;
        std::vector<Decl> decls;
    };

    struct ScopeResolver {
        Arena& a;
        SideTables& tables;
        LocationError& err;

        // 見つからなかった参照は診断にせず数える。未定義の報告は typing の仕事で、
        // ここには判定材料が無い (メンバアクセスの右辺なども混ざるため)。
        std::size_t resolved = 0;
        std::size_t unresolved = 0;

        void resolve(Node<Module> mod);

        // Binder と同じく集成体にしておく (corpus が {arena, tables, err} で作る)。
        // 以下は実装の内側。
        Env* global_ = nullptr;

        void collect(Env& env, const std::vector<Node<Statement>>& stmts, std::size_t base);
        void declare(Env& env, Node<Ident> name, Node<Statement> node, bool is_type,
                     std::size_t position);
        void run_block(Env& env, Node<Body> body, std::size_t base);
        void declare_inline_formats(Env& env, Node<Type> ty, std::size_t position);
        void resolve_name(Env& env, Node<Ident> name, std::size_t position);
        Node<Statement> lookup(const Env& env, std::string_view name, std::size_t position) const;

        // 根 (derive を持たないノード) は複数あり増えもするので、根ごとに口を出さない。
        // NodeAny で受けて as_any<T>() の実行時判定で振り分ける。テンプレートに
        // すると traverse が渡すノード種の数だけ本体が複製される。
        void walk(Env& env, NodeAny n, std::size_t position);

        // 子を一般に辿る。
        void walk_children(Env& env, NodeAny n, std::size_t position);
    };

}  // namespace brgen::nast::bind
