#!/usr/bin/env bash
# Run on Jetson or Apple Silicon Mac. Keeps tokens outside the repository.
set -euo pipefail
umask 077

platform="$(uname -s)-$(uname -m)"
case "$platform" in
  Linux-aarch64)
    asset=go2rtc_linux_arm64
    checksum=359fabade8a7a51e81a55fe6df6b0ef81764a5e1d63179577534eaaa71904b50
    hash_command=(sha256sum)
    ;;
  Darwin-arm64)
    asset=go2rtc_mac_arm64.zip
    checksum=919b78adc759d6b3883d1e1b2ac915ac0985bb903ff1897b4d228527bd64690c
    hash_command=(shasum -a 256)
    command -v unzip >/dev/null
    ;;
  *) echo '支持 Jetson（Linux aarch64）或 Apple Silicon Mac。' >&2; exit 1 ;;
esac
for tool in curl "${hash_command[0]}"; do
  command -v "$tool" >/dev/null || { echo "缺少工具: $tool" >&2; exit 1; }
done

version=v1.9.14
bridge_dir="$HOME/.local/share/coco-ai-camera/go2rtc"
config_dir="$HOME/.config/coco-ai-camera"
binary="$bridge_dir/go2rtc-$version-$platform"
package="$bridge_dir/$version-$asset"
config="$config_dir/go2rtc.yaml"
mkdir -p "$bridge_dir" "$config_dir"
chmod 700 "$bridge_dir" "$config_dir"

if [[ ! -f "$package" ]]; then
  download=$(mktemp "$bridge_dir/download.XXXXXX")
  trap 'rm -f "$download"' EXIT
  curl --fail --location --retry 3 --connect-timeout 15 --max-time 300 \
    "https://github.com/AlexxIT/go2rtc/releases/download/$version/$asset" \
    --output "$download"
  printf '%s  %s\n' "$checksum" "$download" | "${hash_command[@]}" --check --status
  mv "$download" "$package"
  trap - EXIT
fi
printf '%s  %s\n' "$checksum" "$package" | "${hash_command[@]}" --check --status
staged=$(mktemp "$bridge_dir/binary.XXXXXX")
trap 'rm -f "$staged"' EXIT
if [[ "$platform" == Darwin-arm64 ]]; then
  unzip -p "$package" go2rtc > "$staged"
else
  cp "$package" "$staged"
fi
chmod 700 "$staged"
mv "$staged" "$binary"
trap - EXIT

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
echo '管理页面：http://127.0.0.1:1984（本机直接打开，跨电脑访问需 SSH 转发）'
echo '配置保存在 ~/.config/coco-ai-camera/go2rtc.yaml，请勿分享其中的账号凭据。'
echo '按 Ctrl+C 停止。已有配置会保留；首次验证暂不设置开机启动。'
exec "$binary" -config "$config"
