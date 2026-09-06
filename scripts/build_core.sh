#!/usr/bin/env bash
# BeaxtyVPN core build script (sing-box + Xray daemon)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
THRONE_DIR="${ROOT_DIR}/3rdparty/throne"
CORE_SRC_DIR="${THRONE_DIR}/core/server"
BIN_DIR="${ROOT_DIR}/bin"
DEST_BIN="${BIN_DIR}/beaxty-core"

mkdir -p "${BIN_DIR}"
export PATH="${HOME}/go/bin:${PATH}"

echo "==> Ensuring protobuf code generation tools..."
if ! command -v protoc-gen-go &>/dev/null || ! command -v protoc-gen-go-grpc &>/dev/null; then
    go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
    go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
fi

echo "==> Generating protobuf for Throne core daemon..."
cd "${CORE_SRC_DIR}/gen"
protoc -I . --go_out=. --go-grpc_out=. libcore.proto

echo "==> Building BeaxtyVPN core daemon..."
cd "${CORE_SRC_DIR}"
TAGS="with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale"
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)

CGO_ENABLED=1 go build -v \
    -trimpath \
    -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -X 'internal/godebug.defaultGODEBUG=multipathtcp=0' -checklinkname=0" \
    -tags "${TAGS}" \
    -o "${DEST_BIN}" .

chmod +x "${DEST_BIN}"
echo "==> Built core daemon: ${DEST_BIN}"
"${DEST_BIN}" -h || true
