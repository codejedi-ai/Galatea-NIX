#!/usr/bin/env bash
# Load project naming from project.mk (single source of truth).
_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
eval "$(make -s -C "$_ROOT" -f "$_ROOT/project.mk" print-env)"
