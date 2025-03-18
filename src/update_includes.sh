#!/bin/bash

# Update includes in physics files
for file in physics/*.c physics/*.h; do
  # Update core includes
  sed -i '' 's|#include "core_|#include "../core/core_|g' "$file"
  # Update model includes
  sed -i '' 's|#include "model_|#include "../physics/model_|g' "$file"
  # Special case for macros.h, progressbar.h, etc.
  sed -i '' 's|#include "macros.h"|#include "../core/macros.h"|g' "$file"
  sed -i '' 's|#include "progressbar.h"|#include "../core/progressbar.h"|g' "$file"
  sed -i '' 's|#include "sglib.h"|#include "../core/sglib.h"|g' "$file"
  sed -i '' 's|#include "sage.h"|#include "../core/sage.h"|g' "$file"
done

# Update includes in core files
for file in core/*.c core/*.h; do
  # Update model includes
  sed -i '' 's|#include "model_|#include "../physics/model_|g' "$file"
  # Update core includes that need to be from the same directory
  sed -i '' 's|#include "../core/core_|#include "core_|g' "$file"
  sed -i '' 's|#include "../core/macros.h"|#include "macros.h"|g' "$file"
  sed -i '' 's|#include "../core/progressbar.h"|#include "progressbar.h"|g' "$file"
  sed -i '' 's|#include "../core/sglib.h"|#include "sglib.h"|g' "$file"
  sed -i '' 's|#include "../core/sage.h"|#include "sage.h"|g' "$file"
done

# Update includes in io files that reference core or physics
for file in io/*.c io/*.h; do
  # Update core includes
  sed -i '' 's|#include "../core_|#include "../core/core_|g' "$file"
  sed -i '' 's|#include "core_|#include "../core/core_|g' "$file"
  # Update model includes
  sed -i '' 's|#include "../model_|#include "../physics/model_|g' "$file"
  sed -i '' 's|#include "model_|#include "../physics/model_|g' "$file"
  # Special case for macros.h, etc.
  sed -i '' 's|#include "../macros.h"|#include "../core/macros.h"|g' "$file"
  sed -i '' 's|#include "macros.h"|#include "../core/macros.h"|g' "$file"
  sed -i '' 's|#include "../progressbar.h"|#include "../core/progressbar.h"|g' "$file"
  sed -i '' 's|#include "progressbar.h"|#include "../core/progressbar.h"|g' "$file"
  sed -i '' 's|#include "../sglib.h"|#include "../core/sglib.h"|g' "$file"
  sed -i '' 's|#include "sglib.h"|#include "../core/sglib.h"|g' "$file"
  sed -i '' 's|#include "../sage.h"|#include "../core/sage.h"|g' "$file"
  sed -i '' 's|#include "sage.h"|#include "../core/sage.h"|g' "$file"
done

echo "Include paths updated successfully!"
