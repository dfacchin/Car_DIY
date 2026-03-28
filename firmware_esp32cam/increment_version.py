"""Pre-build script: auto-increment ESP32_FW_VERSION in config.h"""
import re
Import("env")

config_path = env.subst("$PROJECT_SRC_DIR") + "/config.h"

with open(config_path, "r") as f:
    content = f.read()

match = re.search(r"#define ESP32_FW_VERSION\s+(\d+)", content)
if match:
    old_ver = int(match.group(1))
    new_ver = old_ver + 1
    content = content.replace(match.group(0), f"#define ESP32_FW_VERSION    {new_ver}")
    with open(config_path, "w") as f:
        f.write(content)
    print(f"ESP32_FW_VERSION: {old_ver} -> {new_ver}")
