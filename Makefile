.PHONY: help
help:   ## Show this help.
	@awk 'BEGIN {FS = ":.*##"; printf "Usage: make \033[36m[target]\033[0m\nWe will pull dependency dirs out of env if exist like LIBSM_DIR\n"} /^[0-9a-zA-Z_-]+:.*?##/ { printf "  \033[36m%-10s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

.PHONY: build
build: ## build
	pio run

.PHONY: flash
flash: ## flash the board
	pio run -t upload

.PHONY: clean
clean: ## clean
	pio run -t clean
