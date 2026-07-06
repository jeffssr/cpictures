# Task 3 Report

Status: DONE

Implemented:
- Added ImageDocumentMetadata.
- Added ImageList with LoadFromFile, Current, Next, Previous, Index, Count, Empty.
- ImageList scans same directory, filters known image formats, sorts naturally, locates current file.
- Fixed LoadFromFile so existing non-image files and unsupported extensions return empty lists, while missing known image paths still return one-item lists.
- Added CMake source entries.
- Added tests for natural order, current index, next/previous navigation, unsupported file filtering.
- Added MSVC /FS for cpictures_core after reproducing PDB write conflict C1041 during parallel build.

Tests:
- RED: `cmake --build --preset debug` failed before implementation with `fatal error C1083: 无法打开包括文件: “cpictures/image_list.h”`.
- GREEN: `cmake --build --preset debug` passed.
- GREEN: `cmake --build --preset debug` passed after fix.
- GREEN: `ctest --preset debug` passed after fix: `100% tests passed, 0 tests failed out of 1`.

Files changed:
- CMakeLists.txt
- include/cpictures/image_document.h
- include/cpictures/image_list.h
- src/image/image_document.cpp
- src/image/image_list.cpp
- tests/core_tests.cpp

Self-review:
- Interfaces match plan names.
- ImageList wraps navigation as expected.
- Build issue root cause was MSVC parallel PDB write; /FS is scoped to cpictures_core and MSVC-only.

Concerns:
- None.
