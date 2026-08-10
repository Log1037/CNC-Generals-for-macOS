#!/bin/zsh
set -e

PROJECT_DIR="${0:A:h}"
export GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-$PROJECT_DIR/游戏文件}"

echo "此步骤会通过 SteamCMD 下载你账户拥有的《命令与征服：将军—零点行动》正版资源。"
echo "首次登录可能需要输入 Steam 密码和 Steam Guard 验证码。"
echo
read "STEAM_USER?请输入 Steam 登录用户名："

if [[ -z "$STEAM_USER" ]]; then
  echo "用户名为空，已取消。"
  read -k 1 "?按任意键关闭……"
  exit 1
fi

cd "$PROJECT_DIR"
./scripts/get-assets.sh "$STEAM_USER"

echo
echo "资源安装完成。现在可以双击“将军：零点行动.app”开始游戏。"
read -k 1 "?按任意键关闭……"
