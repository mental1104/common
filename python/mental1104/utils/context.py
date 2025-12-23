from contextvars import ContextVar 
from dataclasses import dataclass 
from typing import Optional 

@dataclass(frozen=True) 
class RequestCtx: 
    language: str = "zh-CN"
    time_zone: str = "Asia/Shanghai" 

_xdlp_request_ctx: ContextVar[Optional[RequestCtx]] = ContextVar("xdlp_request_ctx", default=None) 

def ctx() -> RequestCtx: 
    v = _xdlp_request_ctx.get() 
    return v if v is not None else RequestCtx() 

def set_ctx(v: RequestCtx): 
    return _xdlp_request_ctx.set(v) 

def reset_ctx(token): 
    _xdlp_request_ctx.reset(token) 
    
# 只用于排障：不暴露私有变量本体 
def ctx_diag(): 
    v = _xdlp_request_ctx.get() 
    return { 
        "module": __name__,
        "ctxvar_id": id(_xdlp_request_ctx),
        "is_set": v is not None,
        "language": (v.language if v else None),
        "time_zone": (v.time_zone if v else None)
    }