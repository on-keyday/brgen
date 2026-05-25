/**
 * @name Fallible accessor used outside MAYBE / explicit check
 * @description Detects calls to rebrgen accessor APIs (`ctx.get`,
 *              `ctx.get_field<>`, `ctx.visit`, `ctx.identifier`) whose result
 *              has `operator!` semantics (pointer or `expected<T>`) but is
 *              not consumed through the `MAYBE(name, expr)` macro nor through
 *              an explicit `if (!x)` / `if (x)` / `x.has_value()` style check.
 *
 *              In rebrgen the convention is that every fallible accessor goes
 *              through MAYBE so nullptr / unexpected propagates as an error
 *              return instead of crashing. AI-generated code often skips this.
 *
 * @kind problem
 * @problem.severity warning
 * @id rebrgen/maybe-missing
 * @tags correctness
 *       rebrgen-custom
 */

import cpp

/** A call to one of the rebrgen accessor APIs whose result is nullable
 *  (raw pointer) or expected-of-something. */
class FallibleAccessorCall extends FunctionCall {
  FallibleAccessorCall() {
    this.getTarget().getName() in ["get", "get_field", "visit", "identifier"] and
    (
      this.getType().getUnspecifiedType() instanceof PointerType
      or this.getType().getUnspecifiedType().toString().matches("expected<%>")
    )
  }
}

/** An invocation of the MAYBE / MAYBE_VOID macro. */
class MaybeMacroInvocation extends MacroInvocation {
  MaybeMacroInvocation() { this.getMacroName() = ["MAYBE", "MAYBE_VOID"] }
}

/** True if `e` lives inside the expansion of a MAYBE / MAYBE_VOID macro. */
predicate insideMaybe(Expr e) {
  exists(MaybeMacroInvocation m | m.getAnExpandedElement() = e)
}

/** True if `e` (or its surrounding sub-expression chain) is consumed by a
 *  truthiness check: `!e`, `if (e)`, `while (e)`, `e ? a : b`,
 *  or `e.has_value()` / `e.has_error()` / `e.value()` / `e.error()`. */
predicate isCheckedExpr(Expr e) {
  exists(NotExpr n | n.getOperand() = e.getParent*())
  or
  exists(IfStmt s | s.getCondition() = e.getParent*())
  or
  exists(WhileStmt s | s.getCondition() = e.getParent*())
  or
  exists(ConditionalExpr s | s.getCondition() = e.getParent*())
  or
  exists(FunctionCall chk |
    chk.getQualifier() = e.getParent*() and
    chk.getTarget().getName() in ["has_value", "has_error", "error", "value"]
  )
}

/** True if the call result reaches an explicit check, either directly or
 *  via a local variable that captures it (`auto x = call(); if (x) ...`). */
predicate explicitlyChecked(Expr call) {
  isCheckedExpr(call)
  or
  // Local variable initialized from the call, later checked.
  exists(LocalVariable v |
    v.getInitializer().getExpr().getFullyConverted() = call.getFullyConverted() and
    isCheckedExpr(v.getAnAccess())
  )
}

/** True if the call is the direct return value (`return call;`) — no
 *  unwrap needed when propagating verbatim. */
predicate directlyReturned(Expr call) {
  exists(ReturnStmt r | r.getExpr() = call.getParent*())
}

from FallibleAccessorCall call
where
  not insideMaybe(call) and
  not explicitlyChecked(call) and
  not directlyReturned(call) and
  // skip obvious test scaffolding
  not call.getEnclosingFunction().getName().matches("test_%")
select call,
  "[REVIEW] Fallible call `" + call.getTarget().getName() +
    "(...)` -> `" + call.getType().getUnspecifiedType().toString() +
    "`: not MAYBE-wrapped and not explicitly checked. Some bypasses are " +
    "intentional (e.g. when the callee handles null input gracefully); " +
    "treat as a review candidate, not a definite bug."
