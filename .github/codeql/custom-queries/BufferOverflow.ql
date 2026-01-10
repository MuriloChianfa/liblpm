/**
 * @name Potential buffer overflow in LPM operations
 * @description Detects potential buffer overflows when copying IP addresses or prefixes
 * @kind path-problem
 * @problem.severity error
 * @security-severity 8.0
 * @precision high
 * @id cpp/liblpm/buffer-overflow
 * @tags security
 *       correctness
 *       external/cwe/cwe-120
 *       external/cwe/cwe-787
 */

import cpp
import semmle.code.cpp.dataflow.TaintTracking
import DataFlow::PathGraph

/**
 * Models functions that take IP address buffers as input
 */
class LpmInputFunction extends Function {
  LpmInputFunction() {
    this.getName() in [
      "lpm_add",
      "lpm_delete", 
      "lpm_lookup",
      "lpm_lookup_ipv4",
      "lpm_lookup_ipv6"
    ]
  }
  
  Parameter getAddressParameter() {
    result = this.getParameter(1) // Usually the second parameter is the address
  }
}

/**
 * Configuration for tracking tainted data from user input to LPM functions
 */
class LpmBufferOverflowConfig extends TaintTracking::Configuration {
  LpmBufferOverflowConfig() { this = "LpmBufferOverflowConfig" }

  override predicate isSource(DataFlow::Node source) {
    // Consider function parameters and external inputs as sources
    exists(Parameter p |
      source.asParameter() = p and
      p.getType().getUnspecifiedType() instanceof PointerType
    )
  }

  override predicate isSink(DataFlow::Node sink) {
    // Sinks are calls to memcpy, memset, or array access in LPM functions
    exists(FunctionCall fc |
      fc.getTarget().getName() in ["memcpy", "memset", "memmove"] and
      sink.asExpr() = fc.getAnArgument()
    )
    or
    exists(ArrayExpr ae |
      sink.asExpr() = ae and
      ae.getFile().getBaseName().matches("lpm%")
    )
  }
}

from LpmBufferOverflowConfig cfg, DataFlow::PathNode source, DataFlow::PathNode sink
where cfg.hasFlowPath(source, sink)
select sink.getNode(), source, sink,
  "Potential buffer overflow: data from $@ may overflow buffer in LPM operation.",
  source.getNode(), "here"
