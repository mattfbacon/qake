#pragma once

#define VERSION "0.1"
#define CURRENT_CONFIG_VERSION 0
#define PREPROC_STR_HELPER(x) #x
#define PREPROC_STR(x) PREPROC_STR_HELPER(x)
#define CURRENT_CONFIG_VERSION_str PREPROC_STR(CURRENT_CONFIG_VERSION)
