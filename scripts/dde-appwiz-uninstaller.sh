#!/bin/bash

# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Check if the path to the .desktop file is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_desktop_file>"
    exit 1
fi

DESKTOP_FILE_PATH="$1"

# Check if the .desktop file exists
if [ ! -f "$DESKTOP_FILE_PATH" ]; then
    echo "Error: File '$DESKTOP_FILE_PATH' does not exist."
    exit 1
fi

# Use dpkg to find the associated package name
PACKAGE_NAME=$(dpkg -S "$DESKTOP_FILE_PATH" 2>/dev/null | awk -F: '{print $1}' | head -n 1)

# Check if a package was found
if [ -z "$PACKAGE_NAME" ]; then
    echo "Error: No package found for the file '$DESKTOP_FILE_PATH'."
    exit 1
fi

# Use pkexec to uninstall the package
if pkexec apt purge -y "$PACKAGE_NAME"; then
    echo "Package '$PACKAGE_NAME' has been successfully uninstalled."
else
    echo "Error: Failed to uninstall the package '$PACKAGE_NAME'."
    exit 1
fi
