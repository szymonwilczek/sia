// Tests are split by feature under tests/<feature>/.
// Functional tests cover user-facing flows; mathtests cover algebraic invariants.

#include "tests/test_support.h"
#include "tests/test_suites.h"

int main(void) {
  printf("=== sia test suite ===\n");
  test_suite_run("lexer:functionaltest", tests_lexer_functional);
  test_suite_run("parser:functionaltest", tests_parser_functional);
  test_suite_run("ast:functionaltest", tests_ast_functional);
  test_suite_run("evaluation:functionaltest", tests_evaluation_functional);
  test_suite_run("numeric:mathtest", tests_numeric_mathtest);
  test_suite_run("factorial:functionaltest", tests_factorial_functional);
  test_suite_run("number_theory:functionaltest", tests_number_theory_functional);
  test_suite_run("number_theory:mathtest", tests_number_theory_mathtest);
  test_suite_run("simplification:functionaltest", tests_simplification_functional);
  test_suite_run("calculus:functionaltest", tests_calculus_functional);
  test_suite_run("calculus:mathtest", tests_calculus_mathtest);
  test_suite_run("trigonometry:functionaltest", tests_trigonometry_functional);
  test_suite_run("trigonometry:mathtest", tests_trigonometry_mathtest);
  test_suite_run("canonical:mathtest", tests_canonical_mathtest);
  test_suite_run("symtab:functionaltest", tests_symtab_functional);
  test_suite_run("matrix:functionaltest", tests_matrix_functional);
  test_suite_run("matrix:mathtest", tests_matrix_mathtest);
  test_suite_run("latex:functionaltest", tests_latex_functional);
  test_suite_run("solver:functionaltest", tests_solver_functional);
  test_suite_run("solver:mathtest", tests_solver_mathtest);

  printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
