#!/usr/bin/env bash
#
# The same R package under two different libsapnwrfc implementations.
#
# A sharper differential than the other bindings get. For pyrfc and node-rfc
# the two sides are different *implementations* of the whole binding, so a
# difference could be anywhere. Here the R and C source is byte for byte
# identical on both sides and only the library underneath changes -- so a
# difference is unambiguously the protocol implementation's.
#
#   SAPNWRFC_HOME=/opt/nwrfcsdk SAP_USER=DEVELOPER SAP_PASSWD=... \
#     bindings/rsap/examples/run_both.sh
#
set -euo pipefail

# Hard-required: without it the script would build both sides against our own
# archive and report "no differences" for a comparison it never made.
: "${SAPNWRFC_HOME:?set it to the unpacked SAP NW RFC SDK, e.g. /opt/nwrfcsdk}"
: "${SAP_USER:?}"
: "${SAP_PASSWD:?}"

HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORK="${WORK:-${TMPDIR:-/tmp}/rsap-side-by-side}"
rm -rf "$WORK" && mkdir -p "$WORK/sap" "$WORK/erpl" "$WORK/lib-sap" "$WORK/lib-erpl"

build() {           # build(dest, libdir, use_sap_sdk)
  local dest="$1" libdir="$2" sap="$3"
  cp -r "$PKG"/. "$dest/src-pkg/" 2>/dev/null || { mkdir -p "$dest/src-pkg"; cp -r "$PKG"/. "$dest/src-pkg/"; }
  rm -rf "$dest/src-pkg/src"/*.o "$dest/src-pkg/src"/*.so
  if [ "$sap" = "1" ]; then
    # LD_LIBRARY_PATH is needed at *install* time too: R loads the freshly
    # built .so to check it, and SAP's libsapnwrfc.so is not on the loader path.
    ( cd "$dest/src-pkg" && RSAP_USE_SAP_SDK=1 \
        LD_LIBRARY_PATH="$SAPNWRFC_HOME/lib" \
        R CMD INSTALL --no-byte-compile --library="$libdir" . >/dev/null )
  else
    ( cd "$dest/src-pkg" && R CMD INSTALL --no-byte-compile --library="$libdir" . >/dev/null )
  fi
}

echo "== building against SAP's libsapnwrfc ($SAPNWRFC_HOME)"
build "$WORK/sap" "$WORK/lib-sap" 1
echo "== building against our libsapnwrfc.a"
build "$WORK/erpl" "$WORK/lib-erpl" 0

echo "== running"
# The SAP side needs the SDK on the loader path. The erpl side is run with it
# explicitly removed -- that asymmetry is the whole claim.
env LD_LIBRARY_PATH="$SAPNWRFC_HOME/lib" R_LIBS="$WORK/lib-sap" \
    Rscript "$HERE/same_program.R" > "$WORK/sap.txt" 2>&1 || true
env -u LD_LIBRARY_PATH R_LIBS="$WORK/lib-erpl" \
    Rscript "$HERE/same_program.R" > "$WORK/erpl.txt" 2>&1 || true

if diff -u "$WORK/sap.txt" "$WORK/erpl.txt" > "$WORK/diff.txt"; then
  printf '\n\033[1mno differences across %s lines\033[0m\n' "$(wc -l < "$WORK/sap.txt")"
else
  echo
  echo "DIFFERENCES (SAP's library on the left, ours on the right):"
  cat "$WORK/diff.txt"
  echo
  echo "outputs: $WORK/sap.txt $WORK/erpl.txt"
  exit 1
fi
