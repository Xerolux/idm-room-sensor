.PHONY: check check-esphome build-esphome build-esp-idf

PLATFORMIO ?= $(shell if command -v platformio >/dev/null 2>&1; then command -v platformio; elif [ -x "$(HOME)/.platformio/penv/bin/platformio" ]; then echo "$(HOME)/.platformio/penv/bin/platformio"; else echo platformio; fi)

check:
	python3 tools/check_repo.py
	python3 tools/dew_point.py 23 60
	python3 -m pytest -q --capture=sys

check-esphome:
	python3 tools/check_esphome.py

build-esphome:
	python3 tools/check_esphome.py --compile

build-esp-idf:
	$(PLATFORMIO) run --project-dir firmware/esp-idf
