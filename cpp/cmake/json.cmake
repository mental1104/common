# 两个后端独立选择；公共节点接口不包含原生 SDK 类型。
option(M1104_ENABLE_JSON_CJSON "Enable the cJSON parser backend" ON)
option(M1104_ENABLE_JSON_RAPIDJSON "Enable the RapidJSON parser backend" ON)
find_path(M1104_RAPIDJSON_INCLUDE_DIR rapidjson/document.h
  PATHS "${PROJECT_SOURCE_DIR}/lib/rapidjson/include")

# 能力必须同时满足总开关、具体后端开关和 SDK 条件；配置结果写入生成头。
set(M1104_HAS_JSON_CJSON FALSE)
set(M1104_HAS_JSON_RAPIDJSON FALSE)
# 总开关开启时，未启用后端的 detail 文件只编译报告不可用的分支。
if(M1104_ENABLE_JSON AND M1104_ENABLE_JSON_CJSON AND TARGET CJSON::lib)
  set(M1104_HAS_JSON_CJSON TRUE)
endif()
if(M1104_ENABLE_JSON AND M1104_ENABLE_JSON_RAPIDJSON AND M1104_RAPIDJSON_INCLUDE_DIR)
  set(M1104_HAS_JSON_RAPIDJSON TRUE)
endif()
set(M1104_HAS_JSON FALSE)
if(M1104_ENABLE_JSON)
  # 公共核心也允许只接入应用自定义后端；内置禁用时保留明确的失败入口。
  set(M1104_HAS_JSON TRUE)
else()
  # 总开关关闭则移除整个运行时，不能让 detail 后端经递归源文件收集漏入主库。
  list(FILTER M1104_CORE_SOURCES EXCLUDE REGEX "/src/json/")
endif()
