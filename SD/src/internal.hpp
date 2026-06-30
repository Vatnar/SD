#pragma once

#ifndef FILE_INTERNAL
#define FILE_INTERNAL _file_local
#endif

#define FILE_INTERNAL_BEGIN \
  namespace {               \
  namespace FILE_INTERNAL {
#define FILE_INTERNAL_END \
  }                       \
  }
