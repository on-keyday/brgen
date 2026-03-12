# C++ Generator Migration G-PDCA Cycle (cpp → cpp3)

_Updated: 2025/04/25_

## Goal

Replace the handwritten C++ generator (`src/old/bm2cpp/`) with a new, generated C++ generator (`src/old/bm2cpp3/`) that leverages the generator-generator (`src/old/bm2/gen_template/`) for continuous improvement.

## Plan

- **Setup:**
  - The `cpp3` entry has been added to `LANG_LIST`.
  - Initial configuration updates (e.g. adjustments in `src/old/bm2cpp3/config.json`) are complete.
- **Analysis:**
  - A diff analysis between the handwritten generator (`src/old/bm2cpp/`) and the new generated version (`src/old/bm2cpp3/`) has been performed.
  - Missing custom logic components have been identified and partially addressed:
    - Advanced handling of union types and member access (e.g. formatting union variants with `std::variant`).
    - Detailed bit field encoding/decoding operations.
    - Customized property getter and setter logic.
- **Action Items:**
  - Continue defining iterative improvements through hooks in `src/old/bm2cpp3/hook/` (using `sections.txt` or additional hook files) to fully address the identified gaps.
  - Verify and update test configurations in `testkit/inputs.json` and ensure build scripts (`cmptest_build.txt` and `cmptest_run.txt`) are current.

## Do

- Implement or refine missing custom logic using the appropriate hooks.
- Regenerate the generator via `script/generate.py` and rebuild/run it with `script/run_generated.py`.

## Check

- Run `script/run_cmptest.py` to validate that tests pass.
- Analyze test outcomes; note any discrepancies to guide further hook refinements.

## Act (Adjust)

- Refine hook implementations based on test feedback.
- Iterate the PDCA cycle until all tests consistently pass.
- Once stable, plan cleanup actions such as removing the old `src/old/bm2cpp/` directory and updating all remaining references.

**Next Step:** Execute `script/run_cmptest.py` to validate the improvements.
