# MarathonRecomp-Prospero — native PS5 build and deployment targets.
# The upstream CMake presets remain for desktop builds; PS5 uses tools/build.sh.

SHELL := /bin/bash
.DEFAULT_GOAL := all

-include .env

TITLE_ID ?= PPSA99903
ifeq ($(strip $(TITLE_ID)),)
TITLE_ID := PPSA99903
endif
PS5_HOST ?=
FTP_PORT ?= 2121
PS5_ELF_PORT ?= 9021
PS5_FTP_USER ?= anonymous
PS5_FTP_PASSWORD ?=
MARATHON_GAME_DIR ?= ressources
MARATHON_RECOMP_RESOURCES ?=
PS5_DEPS_ROOT ?= $(if $(XDG_CACHE_HOME),$(XDG_CACHE_HOME)/MarathonRecomp/deps,$(HOME)/.cache/MarathonRecomp/deps)
PS5_GPU_DIAGNOSTICS ?= 0

.PHONY: all app build deps tools assets resources recomp deploy undeploy launch close clean require-title-id restore-vcpkg

all: app
build: app

require-title-id:
	@if [[ ! "$(TITLE_ID)" =~ ^PPS[AB][0-9]{5}$$ ]]; then \
		echo "Set TITLE_ID to your assigned/fake PPSA/PPSB title ID (five digits)." >&2; \
		exit 2; \
	fi

# Bootstrap the PS5 Payload SDK, static RADV, PacBrew ports, and host recompiler tools.
deps: tools
	@PS5_DEPS_ROOT="$(PS5_DEPS_ROOT)" bash tools/setup-native-dependencies.sh
	@PS5_DEPS_ROOT="$(PS5_DEPS_ROOT)" bash tools/build-radv.sh release
	@PS5_DEPS_ROOT="$(PS5_DEPS_ROOT)" bash tools/setup-pacbrew-dependencies.sh --all >/dev/null

# Build host-side XenonRecomp, XenosRecomp, u8extract, file_to_c, and helper tools.
tools:
	@bash tools/build-host-tools.sh

# Convert the retained sce_sys source artwork/audio into PS5 package assets.
assets:
	@bash tools/prepare-assets.sh

# Generate C source/header pairs from MarathonRecomp's BIN2C resource manifest.
resources:
	@MARATHON_RECOMP_RESOURCES="$(MARATHON_RECOMP_RESOURCES)" bash tools/generate-resources.sh build

# Recompile the supplied Sonic 2006 XEX and shader archives into Marathon sources.
recomp:
	@MARATHON_GAME_DIR="$(MARATHON_GAME_DIR)" bash tools/recomp-xex.sh "$(MARATHON_GAME_DIR)"

# Full PS5 build: bootstrap missing dependencies, regenerate PPC/shaders/resources,
# cross-compile, link, sign, and stage dist/<TITLE_ID> and pkg/<TITLE_ID>.
app: require-title-id
	@PS5_DEPS_ROOT="$(PS5_DEPS_ROOT)" TITLE_ID="$(TITLE_ID)" PS5_GPU_DIAGNOSTICS="$(PS5_GPU_DIAGNOSTICS)" \
		MARATHON_GAME_DIR="$(MARATHON_GAME_DIR)" \
		MARATHON_RECOMP_RESOURCES="$(MARATHON_RECOMP_RESOURCES)" bash tools/build.sh

deploy: require-title-id
	@PS5_DEPS_ROOT="$(PS5_DEPS_ROOT)" TITLE_ID="$(TITLE_ID)" \
		MARATHON_GAME_DIR="$(MARATHON_GAME_DIR)" \
		MARATHON_RECOMP_RESOURCES="$(MARATHON_RECOMP_RESOURCES)" \
		PS5_HOST="$(PS5_HOST)" FTP_PORT="$(FTP_PORT)" \
		PS5_FTP_USER="$(PS5_FTP_USER)" PS5_FTP_PASSWORD="$(PS5_FTP_PASSWORD)" \
		bash tools/deploy.sh deploy

undeploy: require-title-id
	@TITLE_ID="$(TITLE_ID)" PS5_HOST="$(PS5_HOST)" FTP_PORT="$(FTP_PORT)" \
		PS5_FTP_USER="$(PS5_FTP_USER)" PS5_FTP_PASSWORD="$(PS5_FTP_PASSWORD)" \
		bash tools/deploy.sh undeploy

launch: require-title-id
	@PS5_DEPS_ROOT="$(PS5_DEPS_ROOT)" \
		bash tools/send-controller.sh launch "$(TITLE_ID)" "$(PS5_HOST)" "$(PS5_ELF_PORT)"

close: require-title-id
	@PS5_DEPS_ROOT="$(PS5_DEPS_ROOT)" \
		bash tools/send-controller.sh close "$(TITLE_ID)" "$(PS5_HOST)" "$(PS5_ELF_PORT)"

# The PS5 build uses PacBrew, not vcpkg. Restore the upstream vcpkg submodule
# from its compressed workspace snapshot before using the desktop CMake presets.
restore-vcpkg:
	@bash tools/restore-vcpkg.sh

clean:
	@rm -rf build dist pkg tools/bin
	@rm -f MarathonRecomp/version.cpp MarathonRecomp/version.h
