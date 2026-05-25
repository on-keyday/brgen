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

/** A call to std::function::operator=. In C++, `field = lambda` where field
 *  is a std::function is parsed as a member-operator call, NOT an AssignExpr. */
class StdFunctionOpAssignCall extends FunctionCall {
  StdFunctionOpAssignCall() {
    this.getTarget().hasName("operator=") and
    this.getQualifier().getType().getUnspecifiedType().toString().matches("function<%>")
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
     StdFunctionOpAssignCall opasgn, FieldAccess lhs
where
  // The lambda contains a by-ref capture of a dangling-prone local.
  cap = lam.getACapture() and
  v = cap.getInitializer().(VariableAccess).getTarget() and
  // The lambda is the argument to std::function::operator= on a config field.
  opasgn.getArgument(0).getFullyConverted() = lam.getFullyConverted() and
  opasgn.getQualifier() = lhs
select cap,
  "Lambda captures local `" + v.getName() + "` (declared in `" +
    v.getFunction().getName() + "`) by reference and is stored in " +
    "std::function field `" + lhs.getTarget().getName() + "`. " +
    "If the field outlives the enclosing function, the captured reference dangles."
