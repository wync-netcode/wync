.PHONY: *
default:

# ===== basic commands =====

clean:
	rm -rf build

setup:
	meson setup --reconfigure --debug  -Db_ndebug=false --buildtype=debug --prefix=$(CURDIR)/build -Db_coverage=true build

compile:
	meson compile -C build

runtest:
	meson test -C build --verbose

coverage:
	ninja coverage-html -C build


