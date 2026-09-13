build: install_deps
	pebble build

run: build
	pebble install --emulator emery
	pebble install --emulator gabbro
	pebble install --emulator flint

runphone: build
	pebble install --phone "{{env_var('PHONE_IP')}}"

install_deps:
	npm install
	
compile_font: install_deps
	# Compile our font for all digits, the uppercase letters used by the day and
	# month abbreviations, the minus and degree ring the temperature needs, and
	# the point in an abbreviated step count. A character missing from this set
	# is skipped silently at draw time, so "8.2K" would come out as "82K".
	npx --no-install fctx-compiler fonts/BebasNeue-Regular.svg -r '[0-9:A-Z\u00b0.-]'

compile_icon_font: install_deps
	# Build the icon font from the sets listed in fonts/icons.list
	python3 scripts/build-icon-font.py

# Store screenshots, every platform crossed with its own preset list. Leaves
# build/ holding a screenshot build, so `just build` before installing for real.
shots *ARGS: install_deps
	python3 scripts/shots.py {{ARGS}}
