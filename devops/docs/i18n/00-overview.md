# i18n Overview

- Runtime: consume compiled `.mo` only for stability and performance.
- Tools: compile/check helpers for CI and release flows; no dependency on system gettext.
- Layout: standard `<locale>/LC_MESSAGES/<domain>.mo` for compatibility with gettext ecosystem.
