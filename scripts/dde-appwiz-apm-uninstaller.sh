#!/bin/bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-3.0-or-later
# 检查是否提供了文件参数
if [ $# -eq 0 ]; then
    echo "错误: 请提供一个文件作为参数"
    echo "用法: $0 <文件路径>"
    exit 1
fi

FILE="$1"

# 检查文件是否存在
if [ ! -f "$FILE" ]; then
    echo "错误: 文件 '$FILE' 不存在"
    exit 1
fi

# 检查文件中是否包含 X-APM-APPID=
if ! grep -q "X-APM-APPID=" "$FILE"; then
    echo "错误: 文件中未找到 'X-APM-APPID=' 内容"
    exit 1
fi

# 提取 X-APM-APPID= 的值
APPID=$(grep -o "X-APM-APPID=[^ ]*" "$FILE" | head -1 | cut -d'=' -f2)

# 检查是否成功提取到值
if [ -z "$APPID" ]; then
    echo "错误: 无法提取 X-APM-APPID 的值"
    exit 1
fi

echo "找到 APM ID: $APPID"

# 如果不是root用户，则使用pkexec
if [ "$EUID" -ne 0 ]; then
    echo "当前不是root用户，使用 pkexec 提升权限..."
    exec pkexec apm autoremove "$APPID" -y
else
    echo "以root用户身份执行命令..."
    exec apm autoremove "$APPID" -y
fi