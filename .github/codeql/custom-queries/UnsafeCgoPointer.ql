/**
 * @name Unsafe CGo pointer handling
 * @description Detects unsafe Go pointer usage in CGo calls that could lead to memory issues
 * @kind problem
 * @problem.severity warning
 * @security-severity 6.5
 * @precision medium
 * @id go/liblpm/unsafe-cgo-pointer
 * @tags security
 *       correctness
 *       external/cwe/cwe-822
 */

import go

/**
 * Models CGo calls that take unsafe pointers
 */
class CgoCall extends CallExpr {
  CgoCall() {
    exists(Function f |
      this.getTarget() = f and
      f.getName().matches("C.lpm_%")
    )
  }
}

/**
 * Models unsafe pointer conversions
 */
class UnsafePointerConversion extends ConversionExpr {
  UnsafePointerConversion() {
    this.getType().getName() = "unsafe.Pointer"
  }
}

from CgoCall call, UnsafePointerConversion conv
where
  conv.getParent*() = call.getAnArgument() and
  // Check if pointer is from Go memory that could be moved by GC
  exists(UnaryExpr ue |
    ue.getOperator() = "&" and
    ue.getParent*() = conv
  )
select call, 
  "Unsafe CGo call: passing Go pointer to C function $@ may cause memory corruption if GC runs.",
  call.getTarget(), call.getTarget().getName()
