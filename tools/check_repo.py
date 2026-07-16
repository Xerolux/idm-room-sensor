#!/usr/bin/env python3
from pathlib import Path
required=["README.md","PROJECT_STATUS.md","docs","hardware","firmware","homeassistant","validation"]
missing=[x for x in required if not Path(x).exists()]
raise SystemExit("Missing: "+", ".join(missing) if missing else 0)
