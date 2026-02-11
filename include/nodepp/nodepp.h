#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/nodepp.h — Umbrella header for the Node++ framework
// ═══════════════════════════════════════════════════════════════════
//
//  #include "nodepp/nodepp.h"
//  using namespace nodepp;
//
//  This single include gives you everything:
//    • http::createServer()
//    • middleware::bodyParser(), cors(), rateLimiter(), helmet()
//    • graphql::Schema, graphql::createHandler()
//    • console::log(), error(), warn(), info()
//    • fs::readFileSync(), writeFileSync()
//    • path::join(), resolve(), basename()
//    • EventEmitter
//    • JsonValue, NODE_SERIALIZE
//
// ═══════════════════════════════════════════════════════════════════

// Core
#include "json_utils.h"
#include "console.h"
#include "events.h"

// HTTP Server & Middleware
#include "http.h"
#include "middleware.h"
#include "security.h"

// Node.js modules
#include "fs.h"
#include "path.h"

// GraphQL
#include "graphql.h"
