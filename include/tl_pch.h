#pragma once

#define WIN32_LEAN_AND_MEAN // Excludes rarely-used stuff in Windows headers.
#define NOMINMAX // Makes sure Windows.h doesn't define MIN() & MAX() macros.
#include <Windows.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <process.h>
#include <math.h>
#include <float.h>
#include "lz4.h"
