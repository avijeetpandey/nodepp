// ═══════════════════════════════════════════════════════════════════
//  src/graphql.cpp — GraphQL execution engine implementation
// ═══════════════════════════════════════════════════════════════════
//
//  The GraphQL parser and schema are header-only (in graphql.h).
//  This translation unit ensures the symbols are compiled into the
//  library and provides any non-inline implementation.
//
// ═══════════════════════════════════════════════════════════════════

#include "nodepp/graphql.h"

// The GraphQL implementation is fully inline in the header.
// This file exists to:
//   1. Ensure graphql.h compiles cleanly as part of the library
//   2. Provide a place for future non-inline implementations
//   3. Satisfy the CMake source file list

namespace nodepp::graphql {

// Reserved for future non-inline implementations.
// Examples: schema introspection, query validation, subscription support.

} // namespace nodepp::graphql
