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
	# month abbreviations, and the minus and degree ring the temperature needs
	npx --no-install fctx-compiler fonts/BebasNeue-Regular.svg -r '[0-9:A-Z\u00b0-]'

compile_weather_font: install_deps
	# Subset Weather Icons down to the icons listed in fonts/weather-icons.list
	python3 scripts/build-weather-font.py
