# === 自动加载 .env 到 Make 环境（全局生效） =================
REPO_ROOT := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
ENV_SRC    ?= $(REPO_ROOT)/.env
ENV_STAMP  ?= $(REPO_ROOT)/.env.active
ENV_MK     := $(abspath $(ENV_SRC)).mk
ENV_HAVE   := $(wildcard $(ENV_SRC))
ifeq ($(strip $(ENV_HAVE)),)
  ENV_KEYS :=
else
  ENV_KEYS := $(shell awk 'BEGIN{FS="="} /^[[:space:]]*(#|$$)/{next} {k=$$1; sub(/^[[:space:]]*export[[:space:]]+/,"",k); sub(/[[:space:]]+/,"",k); print k}' $(ENV_SRC))
endif

# 仅当 .env 与激活标记同时存在时才导入
ifeq ($(and $(ENV_HAVE),$(wildcard $(ENV_STAMP))),)
  ENV_ACTIVE := 0
else
  ENV_ACTIVE := 1
endif

$(ENV_MK): $(ENV_SRC)
	@set -e
	awk '\
	  /^[[:space:]]*#/ || /^[[:space:]]*$$/ { next } \
	  { line=$$0; sub(/^[[:space:]]*export[[:space:]]+/, "", line); \
	    i=index(line,"="); if(i==0) next; \
	    key=substr(line,1,i-1); val=substr(line,i+1); \
	    sub(/^[[:space:]]+|[[:space:]]+$$/,"",key); \
	    sub(/^[[:space:]]+/,"",val); sub(/[[:space:]]+#.*/, "", val); \
	    if(val ~ /^".*"$$/){sub(/^"/,"",val); sub(/"$$/,"",val)} \
	    else if(val ~ /^'\''.*'\''$$/){sub(/^'\''/,"",val); sub(/'\''$$/,"",val)} \
	    print "export " key " = " val; }' \
	  "$(ENV_SRC)" > "$(ENV_MK)"
	echo "[ok] .env -> $(ENV_MK)"

ifeq ($(ENV_ACTIVE),1)
include $(ENV_MK)
endif

# 若未激活 env，则清空关键环境变量，避免沿用旧值导致连接外部服务
ifeq ($(ENV_ACTIVE),0)
  $(foreach k,$(ENV_KEYS),$(eval override $(k)=))
endif

.PHONY: env-guard env-print env-expose env-clean
env-guard:
	@missing=""; \
	for k in $(ENV_REQ); do eval 'v=$${'$$k':-}'; [ -n "$$v" ] || missing="$$missing $$k"; done; \
	if [ -n "$$missing" ]; then echo "[err] 缺少必需环境变量:$$missing （来源 $(ENV_SRC)）"; exit 2; fi
env-print:
	@env | grep -E '^(PG|POSTGRES|DATABASE_URL|REDIS|PULSAR)=' | sort || true
env-expose:
	@[ -f "$(ENV_MK)" ] && sed -E 's/^export[[:space:]]+([^=[:space:]]+)[[:space:]]*=[[:space:]]*(.*)$/export \1=\2/' "$(ENV_MK)" || true
env-clean:
	if [ -f "$(ENV_STAMP)" ]; then rm -f "$(ENV_STAMP)"; fi; \
	if [ -f "$(ENV_MK)" ]; then rm -f "$(ENV_MK)"; fi; \
	echo "[ok] 已移除导入文件与激活标记：$(ENV_MK) $(ENV_STAMP)"
# ============================================================
