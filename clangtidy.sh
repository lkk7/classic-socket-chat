# In this project C-style code and arrays are unavoidable, so relevant warnings are disabled.
clang-tidy src/*.[ch]pp \
-p "build/CMakeFiles" \
-header-filter=".*" \
-checks="-*,
modernize-*,
cppcoreguidelines-*,
readability-*,
performance-*,
-modernize-use-trailing-return-type,
-cppcoreguidelines-avoid-c-arrays,
-cppcoreguidelines-pro-bounds-array-to-pointer-decay,
-cppcoreguidelines-pro-bounds-pointer-arithmetic,
-cppcoreguidelines-pro-type-cstyle-cast,
-cppcoreguidelines-pro-type-vararg,
-modernize-avoid-c-arrays"
