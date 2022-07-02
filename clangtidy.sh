clang-tidy src/*.[ch]pp \
-p "build/CMakeFiles" \
-header-filter=".*" \
-checks="-*,
modernize-*,
cppcoreguidelines-*,
readability-*,
performance-*,
-modernize-use-trailing-return-type"
# -cppcoreguidelines-pro-bounds-constant-array-index,
# -*-magic-numbers"
