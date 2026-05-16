---
name: Enhancement Feature
about: Propose an enhancement to the SIA project.
title: 'feat: [short title of enhancement]'
labels: enhancement
assignees: ''
---

<!--
Every placeholder is marked as note (`>`). Before submitting an issue, please delete/replace it with your text.
-->


### Context
    Provide context here (delete this placeholder).

    For example:
    To achieve a feature-complete foundational mathematics engine (v1.0.0), the CAS must handle infinite boundaries and singular behaviors with rigorous mathematical semantics. Currently, infinity is treated superficially as a string-based symbol (`AST_VARIABLE` with the name "inf"), which lacks robust algebraic rules and cannot track signs natively. Furthermore, the `limits` module is restricted to two-sided limit evaluations. It cannot differentiate between approaching a point from the left versus the right, making it impossible to correctly evaluate fundamental functions at essential singularities or jump discontinuities (such as `1/x` or `abs(x)/x` at `x=0`).

### Examples of Current Behavior
    Provide examples of current behavior (delete this placeholder), for example:
`sia> lim(1/x, x, 0)`
*Current output:* `inf` (Generic output, masking the essential singularity and lack of two-sided convergence)

`sia> lim(1/x, x, 0+)`
*Current output:* `error: unexpected token: RPAREN`

### Expected Behavior
    Provide examples of current behavior (delete this placeholder), for example:
`sia> lim(1/x, x, 0+)`
*Expected output:* `inf`

`sia> lim(1/x, x, 0-)`
*Expected output:* `-inf`

`sia> lim(abs(x)/x, x, 0-)`
*Expected output:* `-1`

### Proposed Solution
    Provide solution, please be as much detailed as you think you can be. For example (placeholder, delete this):

    Introduce dedicated infrastructure for infinite values and extend the limit pipeline to support directional evaluation:
    1. **First-Class AST Representation:** Introduce explicit `AST_INFINITY` (with an internal sign property for + or -) and `AST_UNDEFINED` nodes into the `AstType` enum, replacing the fragile string-matching convention for "inf".
    2. **Directional Syntax Parsing:** Update the lexer and parser to accept one-sided notation (e.g., `0+`, `0-`, `inf`, `-inf`) or add an optional direction indicator to the limit syntax, storing the direction state directly within the `AST_LIMIT` node.
    3. **Sign-Aware Evaluation Pass:** Modify the evaluation logic in `limits.c` to honor the directional flag when performing substitutions near critical points, tracking sign changes across asymptotes to correctly distinguish between positive and negative infinity.
    4. **Formal Infinity Arithmetic:** Expand `simplify.c` with explicit algebraic rewriting patterns for signed infinities (e.g., `inf + inf -> inf`, `(-1) * inf -> -inf`, `c / inf -> 0`, and flagging `inf - inf` as `AST_UNDEFINED`).

### Tasks / Definition of Done
    Not required, but can be added if that's a tracking issue. In that case, example can be as follows:
    - [ ] **First-Class Infinity & Undefined Nodes:** Implement `AST_INFINITY` (with sign tracking) and `AST_UNDEFINED` in `ast.c/h`, ensuring full propagation support across all traversal and serialization modules.
    - [ ] **One-Sided Limit Parser Support:** Upgrade `lexer.c` and `parser.c` to parse directional qualifiers (`+` / `-` suffixes on targets) and populate the direction attribute within the `AST_LIMIT` structure.
    - [ ] **Directional Evaluation Pass:** Update the core logic in `limits.c` to evaluate left-hand and right-hand paths independently when a directional flag is present.
    - [ ] **Infinity Arithmetic Expansion:** Add rigorous rewrite rules to the simplification engine to handle basic arithmetic operations and indeterminate forms involving the new infinity nodes.
    - [ ] **Calculus Regression Tests:** Add verification tests in `tests/calculus/mathtest.c` covering one-sided limits, vertical asymptotes, and signed infinity arithmetic.
