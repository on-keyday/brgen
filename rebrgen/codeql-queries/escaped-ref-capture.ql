/**
 * @name Lambda by-ref capture of auto-storage local stored into std::function
 * @description Detects lambdas that capture an auto-storage local variable
 *              by reference (`[&]` / `[&v]`) and are subsequently assigned to
 *              a `std::function<...>` field. Such lambdas typically outlive
 *              the function that defined them (the std::function field lives
 *              with its owning struct), so the captured reference dangles
 *              after the enclosing function returns.
 *
 *              In rebrgen this is the shape of `entry_before` hook
 *              registration: `config.X = [&]( ... ) { ... use(local); };`.
 *              These currently happen to be safe only because `dispatch_entry`
 *              keeps `before_ctx` alive throughout `main_logic()`. The hooks
 *              are not invoked after `dispatch_entry` returns. Any future
 *              change that breaks that temporal coupling silently introduces
 *              a use-after-return.
 *
 *              This query is intentionally NOT wired into CI. Run locally on
 *              demand:
 *                codeql database create db --language=cpp --command="python script/build.py"
 *                codeql database analyze db codeql-queries/escaped-ref-capture.ql --format=sarif-latest --output=results.sarif
 *
 * @kind problem
 * @problem.severity warning
 * @id rebrgen/escaped-ref-capture
 * @tags correctness
 *       lifetime
 *       rebrgen-custom
 */

import cpp

/** A capture of a variable by reference inside a lambda expression. */
class ByRefCapture extends LambdaCapture {
  ByRefCapture() { this.isCapturedByReference() }
}

/** A field whose declared type is some instantiation of std::function. */
class StdFunctionField extends Field {
  StdFunctionField() {
    this.getUnspecifiedType().toString().matches("function<%>") or
    this.getUnspecifiedType().toString().matches("std::function<%>")
  }
}

/** A variable that lives on the auto storage of its enclosing function
 *  (i.e. a true local — not a parameter, not static, not a reference). */
class DanglingProneLocal extends LocalScopeVariable {
  DanglingProneLocal() {
    not this instanceof Parameter and
    not this.isStatic() and
    not this.getType() instanceof ReferenceType
  }
}

from LambdaExpression lam, ByRefCapture cap, DanglingProneLocal v,
     AssignExpr asgn, FieldAccess lhs, StdFunctionField field
where
  // The lambda contains a by-ref capture of a dangling-prone local.
  cap = lam.getACapture() and
  v = cap.getField().getVariable()
  and
  // The lambda is assigned (possibly via implicit conversion) into a
  // std::function-typed field.
  asgn.getRValue() = lam.getFullyConverted() and
  asgn.getLValue() = lhs and
  lhs.getTarget() = field
select cap,
  "Lambda captures local `" + v.getName() + "` (declared in `" +
    v.getFunction().getName() + "`) by reference and is stored in " +
    "std::function field `" + field.getName() + "`. " +
    "If the field outlives the enclosing function, the captured reference dangles."
