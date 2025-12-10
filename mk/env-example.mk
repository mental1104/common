# ============ env 模板生成 ============
ENV_EXAMPLE  ?= $(REPO_ROOT)/.env.example

.PHONY: env-example
env-example:
	@if [ -f $(ENV_SRC) ]; then \
		awk '\
			/^[[:space:]]*#/ { print; next } \
			/^[[:space:]]*$$/ { print; next } \
			{ \
				line = $$0; \
				sub(/^[[:space:]]*export[[:space:]]+/, "", line); \
				if (index(line, "=") == 0) { print "#" $$0; next } \
				key = line; \
				sub(/=.*/, "", key); \
				sub(/^[[:space:]]+|[[:space:]]+$$/, "", key); \
				print key "="; \
			} \
		' $(ENV_SRC) > $(ENV_EXAMPLE); \
		echo "[ok] 生成 $(ENV_EXAMPLE)"; \
	else \
		echo "[warn] $(ENV_SRC) 不存在，跳过 env-example"; \
	fi

