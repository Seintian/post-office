#!/usr/bin/env bash
# List project headers missing extern "C" guards.
set -euo pipefail
root="$(dirname "$0")/.."
cd "$root"
headers=$(git ls-files 'include/postoffice/**/*.h' 'libs/postoffice/**/*.h')
missing=()
for h in $headers; do
  if ! grep -q '__cplusplus' "$h"; then
    missing+=("$h")
  fi
done
if ((${#missing[@]}==0)); then
  echo "All headers contain __cplusplus guard." >&2
else
  printf 'Missing extern "C" guards (%d):\n' "${#missing[@]}"
  printf '  %s\n' "${missing[@]}"
fi
