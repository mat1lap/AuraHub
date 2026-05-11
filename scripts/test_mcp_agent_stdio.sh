#!/usr/bin/env bash
set -euo pipefail

# Smoke-test aurahub_test_mcp_agent over a single JSON-RPC line (stdio).
# Usage (from repo root, after build): ./scripts/test_mcp_agent_stdio.sh

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AGENT="${ROOT}/build/mcp/aurahub_test_mcp_agent"

if [[ ! -x "${AGENT}" ]]; then
  echo "Missing ${AGENT}. Build with: cmake --build build --target aurahub_test_mcp_agent" >&2
  exit 1
fi

export AURA_WORKSPACE="${ROOT}"

TMP_LOG="$(mktemp "${TMPDIR:-/tmp}/aura_test_log.XXXXXX")"
export AURA_TEST_AGENT_FILE="${TMP_LOG}"

REQ='{"jsonrpc":"2.0","id":42,"method":"messages/prompt","params":{"text":"smoke test line"}}'
RESP="$(printf '%s\n' "${REQ}" | "${AGENT}")"

echo "${RESP}" | head -c 400
echo ""

if ! grep -q 'assistant_markdown' <<<"${RESP}"; then
  echo "Expected assistant_markdown in response." >&2
  exit 1
fi

if [[ ! -f "${TMP_LOG}" ]]; then
  echo "Expected agent to create log file at ${TMP_LOG}" >&2
  exit 1
fi

grep -q 'smoke test line' "${TMP_LOG}"
echo "OK: log written to ${TMP_LOG}"
rm -f "${TMP_LOG}"
