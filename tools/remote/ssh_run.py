"""Send an exact UTF-8 Bash script to SSH without nested shell quoting."""
import argparse
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--host", required=True)
parser.add_argument("--key", type=Path, required=True)
parser.add_argument("--script", type=Path, required=True)
args = parser.parse_args()
text = args.script.read_text("utf-8").replace("\r\n", "\n")
command = ["ssh", "-i", str(args.key), "-o", "BatchMode=yes", "-o", "ConnectTimeout=15",
           "-o", "ServerAliveInterval=30", "-o", "ServerAliveCountMax=3", args.host, "bash -s"]
result = subprocess.run(command, input=text.encode("utf-8"))
sys.exit(result.returncode)
