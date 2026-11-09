ARDUINO_CLI ?= arduino-cli
PYTHON ?= python3

.PHONY: test firmware package release

test:
	$(PYTHON) -c "from importlib.metadata import version; assert version('pymysensors') == '0.26.0', 'Install requirements-test.txt first'"
	$(PYTHON) -m unittest discover -s tests -v

firmware:
	$(ARDUINO_CLI) compile --profile uno --warnings all --build-path "$(CURDIR)/.build/uno" firmware/GeigerMySensors
	mkdir -p dist
	cp .build/uno/GeigerMySensors.ino.hex dist/GeigerMySensors.hex

package: test firmware
	$(PYTHON) tools/package.py

release: package
