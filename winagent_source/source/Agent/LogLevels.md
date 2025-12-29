# SyslogAgent Log Levels

## Debug Levels (Most to Least Verbose)
- **DEBUG3**: Most verbose debug level
  - Used for detailed tracing and variable state
  - Function parameters and return values
  - Memory allocation/deallocation details
  
- **DEBUG2**: Intermediate debug level
  - Function entry/exit points
  - State changes and transitions
  - Important variable updates
  
- **DEBUG**: Basic debug level
  - Important flow control points
  - Significant state information
  - Configuration changes

- **VERBOSE**: General operational information
  - Not debug-specific but more detailed than INFO
  - Useful for understanding normal operation
  - Connection status and statistics

## Normal Operation Levels
- **INFO**: Normal operational messages
  - Status updates
  - Expected state changes
  - Service start/stop
  - Configuration loads

- **WARN**: Warning conditions
  - Non-critical issues
  - Performance degradation
  - Resource usage warnings
  - Approaching limits

## Error Levels (Least to Most Severe)
- **RECOVERABLE_ERROR**: Errors that can be retried or skipped
  - Network timeouts
  - Queue full conditions
  - Buffer allocation failures
  - Temporary resource exhaustion

- **CRITICAL**: Severe problems requiring manual intervention
  - Service interruption
  - Configuration errors
  - Resource exhaustion
  - Component failures that don't force termination

- **FATAL**: Unrecoverable errors forcing program termination
  - Invalid program state
  - Core component initialization failure
  - Unhandled exceptions
  - Critical resource failures

## Special Levels
- **NOLOG**: Configuration setting only
  - Disables all logging except FORCE
  - Not used in code, only in configuration
  - Useful for completely disabling logging

- **ALWAYS**: Messages that appear at every log level
  - Appears at all log levels except NOLOG
  - Important service state changes
  - Critical security events
  - Essential operational messages

- **FORCE**: Messages that bypass all filters
  - Logged regardless of log level settings
  - Critical security breaches
  - Service termination
  - Fatal error conditions

## Usage Guidelines
1. Use the most appropriate level for the situation
2. Don't use DEBUG levels in production unless investigating issues
3. RECOVERABLE_ERROR should be used when the error can be handled
4. CRITICAL indicates service is impaired but running
5. FATAL should only be used when the program must terminate
6. ALWAYS/FORCE should be used sparingly for truly critical messages
