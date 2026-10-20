#!/usr/bin/env bash
# Downloads a RouteViews prefix-to-AS snapshot published by CAIDA and converts it to the routing table
# format the tools read: one "prefix,next_hop" row per prefix, with the origin AS as the next hop.
#
#   scripts/fetch_routeviews.sh [YYYYMMDD-HHMM]    (default 20261001-1200)
#
# Data: Routeviews Prefix to AS mappings Dataset for IPv4 and IPv6,
# https://www.caida.org/catalog/datasets/routeviews-prefix2as/
set -euo pipefail
cd "$(dirname "$0")/.."

stamp=${1:-20261001-1200}
url="https://publicdata.caida.org/datasets/routing/routeviews-prefix2as/${stamp:0:4}/${stamp:4:2}/routeviews-rv2-$stamp.pfx2as.gz"
out="data/routeviews/routeviews-$stamp.csv"

mkdir -p data/routeviews
curl -fsSL "$url" | gunzip | awk -F'\t' '
  BEGIN { print "IP_Prefix,Router" }
  NF == 3 { split($3, origin, /[_,]/); print $1 "/" $2 ",AS" origin[1] }' > "$out.tmp"
mv "$out.tmp" "$out"
echo "$out: $(($(wc -l < "$out") - 1)) prefixes"
