// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant
// https://github.com/SlickQuant/slick-socket

#pragma once

// Logging function placeholders
// User can assign their own log functions by defining these macros before including this file
#ifndef LOG_DEBUG
#define LOG_DEBUG(...) do {} while(0)
#endif
#ifndef LOG_INFO  
#define LOG_INFO(...) do {} while(0)
#endif
#ifndef LOG_WARN
#define LOG_WARN(...) do {} while(0)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(...) do {} while(0)
#endif
#ifndef LOG_TRACE
#define LOG_TRACE(...) do {} while(0)
#endif
