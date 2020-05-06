/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "TypeInference.h"

/*
 * This class takes a method, infers the type of all registers and checks that
 * all operations are well typed. The inferred types are available via the
 * `get_type` method and can be used by optimization/analysis passes that
 * require type information. Note that the type checker stops at the first error
 * encountered.
 *
 * IMPORTANT: the type checker assumes that invoke-* instructions are in
 * denormalized form, i.e., wide arguments are explicitly represented by a pair
 * of consecutive registers. The type checker doesn't modify the IR and hence,
 * can be used anywhere in Redex.
 */
class IRTypeChecker final {

  using TypeEnvironment = type_inference::TypeEnvironment;

 public:
  // If we don't declare a destructor for this class, a default destructor will
  // be generated by the compiler, which requires a complete definition of
  // TypeInference, thus causing a compilation error. Note that the destructor's
  // definition must be located after the definition of TypeInference.
  ~IRTypeChecker();

  explicit IRTypeChecker(DexMethod* dex_method, bool validate_access = false);

  IRTypeChecker(const IRTypeChecker&) = delete;

  IRTypeChecker& operator=(const IRTypeChecker&) = delete;

  /*
   * TOP represents an undefined value and hence, should never occur as the type
   * of a register. However, the Android verifier allows one exception, when an
   * undefined value is used as the operand of a move-* instruction (TOP is
   * named 'conflict' in the dataflow framework used by the Android verifier):
   *
   * http://androidxref.com/7.1.1_r6/xref/art/runtime/verifier/register_line-inl.h#101
   *
   * By default, the type checker complies with the Android verifier. Calling
   * this method enables a stricter check of move-* instructions: using a
   * register holding an undefined value in a move-* will result into a type
   * error.
   */
  void verify_moves() {
    if (!m_complete) {
      // We can only set this parameter before running the type checker.
      m_verify_moves = true;
    }
  }

  /*
   * ART has various issues that get triggered by code overwriting the `this`
   * register, even if the `this` pointer isn't live-out. See
   * `canHaveThisTypeVerifierBug` and `canHaveThisJitCodeDebuggingBug` in r8's
   * InternalOptions.java for details.
   */
  void check_no_overwrite_this() {
    if (!m_complete) {
      // We can only set this parameter before running the type checker.
      m_check_no_overwrite_this = true;
    }
  }

  void run();

  bool good() const {
    check_completion();
    return m_good;
  }

  bool fail() const {
    check_completion();
    return !m_good;
  }

  /*
   * Returns a legible description of the type error, or "OK" otherwise. Note
   * that type checking aborts at the first error encountered.
   */
  std::string what() const {
    check_completion();
    return m_what;
  }

  /*
   * Returns the type of a register at the given instruction. Note that the type
   * returned is that of the register _before_ the instruction is executed. For
   * example, if we query the type of v0 in the following instruction:
   *
   *   aget-object v0, v1, v0
   *
   * we will get INT and not REFERENCE, which would be the type of v0 _after_
   * the instruction has been executed.
   */
  IRType get_type(IRInstruction* insn, reg_t reg) const;

  boost::optional<const DexType*> get_dex_type(IRInstruction* insn,
                                               reg_t reg) const;

 private:
  void check_completion() const {
    always_assert_log(m_complete,
                      "The type checker did not run on method %s.\n",
                      m_dex_method->get_deobfuscated_name().c_str());
  }

  void assume_scalar(TypeEnvironment* state,
                     reg_t reg,
                     bool in_move = false) const;
  void assume_reference(TypeEnvironment* state,
                        reg_t reg,
                        bool in_move = false) const;
  void check_instruction(IRInstruction* insn,
                         TypeEnvironment* current_state) const;

  DexMethod* m_dex_method;
  const bool m_validate_access;
  bool m_complete;
  bool m_verify_moves;
  bool m_check_no_overwrite_this;
  bool m_good;
  std::string m_what;
  std::unique_ptr<type_inference::TypeInference> m_type_inference;

  friend std::ostream& operator<<(std::ostream&, const IRTypeChecker&);
};

std::ostream& operator<<(std::ostream& output, const IRTypeChecker& checker);
