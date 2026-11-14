"""Compile the test image only. Never open serial, upload or erase flash."""
from pathlib import Path
import importlib.util
import os
import subprocess
import sys

root = Path(__file__).resolve().parent
if importlib.util.find_spec('platformio') is None:
    raise SystemExit('PlatformIO missing. Install platformio==6.1.19 in this Python environment.')
env = os.environ.copy()
env.update({
    'WIFI_ENABLED': '1', 'WEBSERVER_ENABLED': '1', 'MQTT_ENABLED': '1',
    'WIFI_SSID': '', 'WIFI_PASSWORD': '', 'MQTT_MAX_PACKET_SIZE': '1024',
    'PROG_VERSION': '1.8.7-volets-stabilite4-sdb',
})
subprocess.run([sys.executable, '-m', 'platformio', 'run', '-e', 'd1_mini', '-d', str(root)],
               env=env, check=True)
print('Build only: Wemos was not accessed.')
