#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include "tcp_server_win32.h"
#else
#include "tcp_server_unix.h"
#endif