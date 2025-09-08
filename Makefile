.PHONY: *
default:

# ===== basic commands =====

clean:
	rm -rf build

setup:
	meson setup --reconfigure --debug  -Db_ndebug=false --buildtype=debug --prefix=$(CURDIR)/build -Db_coverage=true build

setupASAN:
	meson setup --reconfigure --debug  -Db_ndebug=false --buildtype=debug --prefix=$(CURDIR)/build -Db_coverage=true build -Db_sanitize=address -Db_lundef=false

compile:
	meson compile -C build

runtest:
	meson test -C build --verbose

coverage:
	ninja coverage-html -C build


