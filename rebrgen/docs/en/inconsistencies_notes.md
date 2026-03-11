## 8. Inconsistencies/Notes for Review

### 8.1 "How to Add Language" in `README.md`
The "How to Add Language" section in the top-level `README.md` describes a manual, step-by-step process involving `src2json`, `ebmgen`, and `ebm2<lang name>` executions. This approach is partially outdated as the `script/unictest.py` framework now automates much of this process, especially for testing and verifying new language implementations. The new documentation should emphasize the `unictest.py` approach as the primary workflow for developing and testing language generators, while keeping the underlying manual command sequence as an explanation of the internal mechanism or for specific debugging scenarios.

### 8.2 General Note on "Old Generation" Scripts
Many scripts in `commands.md` are marked as `[旧世代]bmgen`. The new documentation should clearly separate and de-emphasize these older scripts, focusing primarily on the `ebmgen`/`ebmcodegen` and `unictest.py` related tools and workflows.
