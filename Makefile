PORT ?= /dev/cu.usbserial-2130
TARGET ?= esp32

.PHONY: help build flash monitor flash-monitor chat login font clean fullclean menuconfig set-target ports

help:
	@echo "Crystal commands:"
	@echo "  make build          Build project"
	@echo "  make flash          Flash to board on $(PORT)"
	@echo "  make monitor        Open serial monitor on $(PORT)"
	@echo "  make flash-monitor  Flash then monitor on $(PORT)"
	@echo "  make chat           Chat with Crystal in the terminal"
	@echo "  make login          Sign Codex in to your ChatGPT account (once)"
	@echo "  make font           Regenerate main/font_nunito.c"
	@echo "  make clean          Clean build files"
	@echo "  make fullclean      Fully reset build directory"
	@echo "  make menuconfig     Open ESP-IDF config menu"
	@echo "  make set-target     Set chip target, default $(TARGET)"
	@echo "  make ports          List serial ports"
	@echo ""
	@echo "Override port example:"
	@echo "  make flash-monitor PORT=/dev/cu.usbserial-211310"

build:
	eim run "idf.py build"

flash:
	eim run "idf.py -p $(PORT) flash"

monitor:
	eim run "idf.py -p $(PORT) monitor"

flash-monitor:
	eim run "idf.py -p $(PORT) flash monitor"

chat:
	python3 tools/crystal_chat.py --port $(PORT)

login:
	./node_modules/.bin/codex login

font:
	.venv/bin/python tools/gen_font.py --ttf tools/fonts/Nunito-ExtraBold.ttf --size 22 --out main/font_nunito.c

clean:
	eim run "idf.py clean"

fullclean:
	eim run "idf.py fullclean"

menuconfig:
	eim run "idf.py menuconfig"

set-target:
	eim run "idf.py set-target $(TARGET)"

ports:
	@ls /dev/cu.*
