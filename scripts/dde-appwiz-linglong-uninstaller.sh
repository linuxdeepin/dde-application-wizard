#!/bin/bash

# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Check if the Linglong App ID is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <linglong_app_id>"
    exit 1
fi

LINGLONG_APP_ID="$1"

# Check if ll-cli is available
if ! command -v ll-cli &> /dev/null; then
    echo "Error: ll-cli is not available on this system."
    exit 1
fi

# Check if the Linglong bundle is installed
if ! ll-cli list | grep -q "$LINGLONG_APP_ID"; then
    echo "Error: Linglong bundle '$LINGLONG_APP_ID' is not installed."
    exit 1
fi

# Uninstall the Linglong bundle
if ll-cli uninstall "$LINGLONG_APP_ID"; then
    echo "Linglong bundle '$LINGLONG_APP_ID' has been successfully uninstalled."
else
    echo "Error: Failed to uninstall the Linglong bundle '$LINGLONG_APP_ID'."
    exit 1
fi
