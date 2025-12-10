.PHONY: _docker-up-all-if-needed _docker-down-all-if-needed
_docker-up-all-if-needed:
	@if [ -n "$(DOCKER_DISABLED)" ]; then \
		echo "[skip] macOS 检测到，跳过 setup-docker"; \
	else \
		$(MAKE) --no-print-directory setup-docker; \
	fi

_docker-down-all-if-needed:
	@if [ -n "$(DOCKER_DISABLED)" ]; then \
		echo "[skip] macOS 检测到，跳过 clean-docker"; \
	else \
		$(MAKE) --no-print-directory clean-docker; \
	fi

