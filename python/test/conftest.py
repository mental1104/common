#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Ensure commonly used plugins are loaded for all tests.
pytest_plugins = (
    "pytest_mock",
    "pytest_asyncio.plugin",
)
