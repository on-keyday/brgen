/**
 * @name (review aid) By-ref capture into std::function, including reference captures
 * @description Loose variant of `escaped-ref-capture.ql`. Surfaces ALL by-ref
 *              captures (including reference parameters / reference locals)
 *              of any lambda assigned to a `std::function<...>` field.
 *
 *              Many findings here will be technically safe because the
 *              captured value is itself a reference to something with longer
 *              lifetime (e.g. `Context_entry_before& ctx` captured `[&]`
 *              actually captures the referent, which `dispatch_entry` keeps
 *              alive across `main_logic()`). They are listed so the human
 *              reviewer can confirm the lifetime invariant still holds —
 *              future refactors that break that invariant would create
 *              silent UAF.
 *
 *              For "definitely escaped local" alerts (no false positives by
 *              construction), use the strict sibling query `escaped-ref-capture.ql`.
 *
 *              Run locally on demand:
 *                codeql database analyze save/codeql-db codeql-queries/escaped-ref-capture-review.ql \
 *                  --format=sarif-latest --output=save/codeql-review.sarif
 *
 * @kind problem
 * @problem.severity recommendation
 * @id rebrgen/escaped-ref-capture-review
 * @tags review-aid
 *       lifetime
 *       rebrgen-custom
 */

import cpp

/** A call to `std::function::operator=`. In C++ `field = lambda` where field
 *  is a `std::function` is parsed as a member-operator call, NOT an AssignExpr. */
class StdFunctionOpAssignCall extends FunctionCall {
  StdFunctionOpAssignCall() {
    this.getTarget().hasName("operator=") and
    this.getQualifier().getType().getUnspecifiedType().toString().matches("function<%>")
  }
}

from LambdaExpression lam, LambdaCapture cap, Variable v,
     StdFunctionOpAssignCall opasgn, FieldAccess lhs
where
  cap.isCapturedByReference() and
  cap = lam.getACapture() and
  v = cap.getInitializer().(VariableAccess).getTarget() and
  opasgn.getArgument(0).getFullyConverted() = lam.getFullyConverted() and
  opasgn.getQualifier() = lhs
select cap,
  "[REVIEW] By-ref capture of `" + v.getName() + "` (type `" +
    v.getType().toString() + "`) flows into std::function field `" +
    lhs.getTarget().getName() + "`. Confirm the captured value outlives the field."
