# Test layout

Tests are grouped by feature under `tests/<feature>/`.

- `functionaltest.c` covers user-facing flows and realistic usage paths.
- `mathtest.c` covers algebraic identities, numeric invariants, and theory-level assumptions.

`test_sia.c` is the suite orchestrator. Shared assertions and helpers live in
`tests/test_support.*`, and suite declarations live in `tests/test_suites.h`.
The Makefile compiles every `tests/*/*.c` module automatically.
