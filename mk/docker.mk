# =================== Docker（仅扫描 images/） ===================
COMPOSE_BIN        ?= docker compose
COMPOSE_FILE_NAME  ?= docker-compose.yaml
COMPOSE_DIRS := $(shell find $(REPO_ROOT)/images -type f -name $(COMPOSE_FILE_NAME) -exec dirname {} \; | sort -u)
ENV_FILE_OPT := $(shell [ -f "$(ENV_SRC)" ] && printf -- '--env-file %s' "$(ENV_SRC)")

.PHONY: setup-docker clean-docker
setup-docker: $(ENV_MK)
	@touch "$(ENV_STAMP)"
	$(call __docker_up_all)

clean-docker:
	$(call __docker_down_all)
	@rm -f "$(ENV_STAMP)" "$(ENV_MK)" || true

