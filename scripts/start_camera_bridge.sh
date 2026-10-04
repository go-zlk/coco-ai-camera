#!/usr/bin/env bash
# Run on the Jetson. Keeps account tokens outside the repository.
set -euo pipefail
umask 077

if [[ "$(uname -s)" != Linux || "$(uname -m)" != aarch64 ]]; then
  echo '请在 Jetson（Linux aarch64）上运行此脚本。' >&2
  exit 1
fi
for tool in curl sha256sum; do
  command -v "$tool" >/dev/null || { echo "缺少工具: $tool" >&2; exit 1; }
done

version=v1.9.14
checksum=359fabade8a7a51e81a55fe6df6b0ef81764a5e1d63179577534eaaa71904b50
bridge_dir="$HOME/.local/share/coco-ai-camera/go2rtc"
config_dir="$HOME/.config/coco-ai-camera"
binary="$bridge_dir/go2rtc-$version"
config="$config_dir/go2rtc.yaml"
mkdir -p "$bridge_dir" "$config_dir"
chmod 700 "$bridge_dir" "$config_dir"

if [[ ! -f "$binary" ]]; then
  download=$(mktemp "$bridge_dir/download.XXXXXX")
  trap 'rm -f "$download"' EXIT
  curl --fail --location --retry 3 --connect-timeout 15 --max-time 300 \
    "https://github.com/AlexxIT/go2rtc/releases/download/$version/go2rtc_linux_arm64" \
    --output "$download"
  printf '%s  %s\n' "$checksum" "$download" | sha256sum --check --status
  chmod 700 "$download"
  mv "$download" "$binary"
  trap - EXIT
fi
printf '%s  %s\n' "$checksum" "$binary" | sha256sum --check --status

if [[ ! -f "$config" ]]; then
  cat > "$config" <<'YAML'
# Access the admin UI via SSH forwarding. Keep these listeners local.
api:
  listen: "127.0.0.1:1984"
rtsp:
  listen: "127.0.0.1:8554"
webrtc:
  listen: ""
streams: {}
YAML
fi
chmod 600 "$config"
echo '管理页面：http://127.0.0.1:1984（电脑访问需 SSH 转发）'
echo '配置保存在 ~/.config/coco-ai-camera/go2rtc.yaml，请勿分享其中的账号凭据。'
echo '按 Ctrl+C 停止。已有配置会保留；首次验证暂不设置开机启动。'
exec "$binary" -config "$config"
