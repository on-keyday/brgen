/*license*/
#include "import_resolver.hpp"

#include "../stream.h"
#include "../traverse.h"

namespace brgen::nast::bind {

    void ImportResolver::resolve(Node<Module> root) {
        if (!root) {
            return;
        }
        auto* h = a.header_at(root.id());
        if (!h) {
            return;
        }
        modules.push_back(root);
        done_.emplace(h->loc.file, root);
        stack_.push_back(h->loc.file);
        walk(root);
        stack_.pop_back();
    }

    void ImportResolver::walk(Node<Module> mod) {
        // Import を集めてから処理する。handle が同じアリーナに新しいノードを
        // 足すので、走査中に足すと visit_all の足元が動く。
        std::vector<Node<Import>> found;
        visit_all(a, mod, [&](auto n) {
            if (auto import_ = n.template as_any<Import>()) {
                found.push_back(import_);
            }
            return true;
        });
        for (auto& import_ : found) {
            handle(import_);
        }
    }

    void ImportResolver::handle(Node<Import> import_) {
        auto* d = a.get<Import>(import_);
        auto* h = a.header_at(import_.id());
        if (!d || !h) {
            return;
        }
        // 相対パスは import する側のファイルからの相対。
        auto base = files.get_path(h->loc.file).parent_path();
        auto added = files.add_file(d->path, true, base);
        if (!added) {
            (void)err.error(h->loc, "cannot open ", d->path, ": ",
                            brgen::to_error_message(added.error()));
            failed++;
            return;
        }
        auto index = *added;

        for (auto& on_stack : stack_) {
            if (on_stack == index) {
                (void)err.error(h->loc, "circular import: ", d->path);
                failed++;
                return;
            }
        }

        if (auto it = done_.find(index); it != done_.end()) {
            tables.table<ImportResolution>().set(import_, ImportResolution{.module = it->second});
            resolved++;
            return;
        }

        auto* input = files.get_input(index);
        if (!input) {
            (void)err.error(h->loc, "cannot read ", d->path);
            failed++;
            return;
        }
        // Context は Stream を 1 本しか持たないので、ファイルごとに作る。
        // 使い回すと外側のファイルの読み位置が壊れる。
        Context ctx;
        auto parsed = ctx.enter_stream(input, [&](Stream& s) {
            return parse(a, s, &err, option);
        });
        if (!parsed) {
            for (auto& ent : parsed.error().locations) {
                err.locations.push_back(std::move(ent));
            }
            failed++;
            return;
        }

        done_.emplace(index, *parsed);
        modules.push_back(*parsed);
        tables.table<ImportResolution>().set(import_, ImportResolution{.module = *parsed});
        resolved++;

        stack_.push_back(index);
        walk(*parsed);
        stack_.pop_back();
    }

}  // namespace brgen::nast::bind
