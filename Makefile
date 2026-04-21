CONTAINER_ENGINE ?= $(shell command -v docker 2>/dev/null || command -v podman 2>/dev/null)
CONTAINER_READY ?= $(shell if [ -n "$(CONTAINER_ENGINE)" ] && $(CONTAINER_ENGINE) info >/dev/null 2>&1; then echo yes; fi)
PREFIX ?= /usr/local
VERSION ?= $(shell git tag | grep '^v' | cut -d 'v' -f 2 | sort -nr | head -n 1)

all: build

build:
	cmake -S src -B build
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure
	$(MAKE) test-musl test-glibc

test-musl:
	@if [ -z "$(CONTAINER_ENGINE)" ]; then echo "Skipping $@: no container engine found"; \
	elif [ "$(CONTAINER_READY)" != "yes" ]; then echo "Skipping $@: container engine unavailable"; \
	else $(CONTAINER_ENGINE) run --rm -v $(CURDIR):/stdred:ro alpine:edge sh -c '\
		set -e && \
		apk add --no-cache cmake make gcc musl-dev python3 zsh >/dev/null 2>&1 && \
		cmake -S /stdred/src -B /build && \
		cmake --build /build && \
		ctest --test-dir /build --output-on-failure'; fi

test-glibc:
	@if [ -z "$(CONTAINER_ENGINE)" ]; then echo "Skipping $@: no container engine found"; \
	elif [ "$(CONTAINER_READY)" != "yes" ]; then echo "Skipping $@: container engine unavailable"; \
	else $(CONTAINER_ENGINE) run --rm -v $(CURDIR):/stdred:ro debian:sid sh -c '\
		set -e && \
		apt-get update -qq && apt-get install -y -qq cmake gcc libc6-dev python3 zsh >/dev/null 2>&1 && \
		cmake -S /stdred/src -B /build && \
		cmake --build /build && \
		ctest --test-dir /build --output-on-failure'; fi

install: build
	cmake --install build --prefix "$(PREFIX)"

clean:
	rm -rf build

dist_prepare: test
	mkdir -p usr/share/doc/stdred && cp README.md usr/share/doc/stdred/

package_deb: dist_prepare
	rm -f *.deb
	mkdir -p usr/bin usr/share/stdred
	cp build/stdred usr/bin/
	cp stdred.plugin.zsh usr/share/stdred/
	fpm -s dir -t deb -n stdred -v $(VERSION) --license MIT --vendor 'Paul' -m 'Paul <devnull@example.com>' --description "Wrap interactive zsh commands and colorize stderr via a PTY helper" --url https://github.com/devnoname120/zsh-stdred usr/bin/stdred usr/share/stdred/stdred.plugin.zsh usr/share/doc/stdred/README.md

package_rpm_64: dist_prepare
	rm -f *.rpm
	mkdir -p usr/bin usr/share/stdred
	cp build/stdred usr/bin/
	cp stdred.plugin.zsh usr/share/stdred/
	fpm -s dir -t rpm -n stdred -v $(VERSION) --license MIT --vendor 'Paul' -m 'Paul <devnull@example.com>' --description "Wrap interactive zsh commands and colorize stderr via a PTY helper" --url https://github.com/devnoname120/zsh-stdred usr/bin/stdred usr/share/stdred/stdred.plugin.zsh usr/share/doc/stdred/README.md
