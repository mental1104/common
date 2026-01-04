-- bootstrap lazy.nvim, LazyVim and your plugins

require("config.lazy")

local function get_git_user()
    local git_user = vim.fn.systemlist("git config --global user.name")[1]
    if git_user and git_user ~= "" then return git_user end

    local env_author = vim.fn.getenv("GIT_AUTHOR_NAME")
    if env_author and env_author ~= "" then return env_author end

    local env_committer = vim.fn.getenv("GIT_COMMITTER_NAME")
    if env_committer and env_committer ~= "" then return env_committer end

    return "Unknown Author"
end

local function get_current_time()
    return os.date("%Y-%m-%d %H:%M:%S")
end

-- 生成注释头部，返回“行数组”
local function get_git_header(ext)
    local user = get_git_user()
    local time = get_current_time()
    local header = string.format("Date: %s\nAuthor: %s\nLastEditors: %s\nLastEditTime: %s", time, user, user, time)
    local comment_styles = {
        py = "#", sh = "#", go = "//",
        c = "/*", h = "/*", cpp = "/*", cc = "/*", hh = "/*", hpp = "/*"
    }
    local comment_start = comment_styles[ext]
    local lines = {}
    if comment_start == "/*" then
        table.insert(lines, "/*")
        for line in header:gmatch("[^\n]+") do
            table.insert(lines, " * " .. line)
        end
        table.insert(lines, " */")
    else
        for line in header:gmatch("[^\n]+") do
            table.insert(lines, comment_start .. " " .. line)
        end
    end
    return lines
end

local function get_header_for_h_file(filename)
    -- 原始的宏定义
    local macro = string.upper(filename:gsub("%.", "_")) .. "_H"
    -- 提示输入保护头前缀（留空则使用默认）
    local prefix = vim.fn.input("请输入保护头前缀（留空使用默认）：")
    if prefix and prefix ~= "" then
        prefix = string.upper(prefix)
        macro = prefix .. "_" .. macro
    end

    -- 先生成头部的注释块（使用块注释风格）
    local header_lines = get_git_header("h")
    local lines = {}
    for _, line in ipairs(header_lines) do
        table.insert(lines, line)
    end

    -- 在注释块后添加一个空行
    table.insert(lines, "")
    -- 添加 header guard 部分
    table.insert(lines, "#ifndef " .. macro)
    table.insert(lines, "#define " .. macro)
    -- 在 #define 与 #endif 之间插入三行空行（光标将定位在中间那一行）
    table.insert(lines, "")
    table.insert(lines, "")
    table.insert(lines, "")
    table.insert(lines, "#endif // " .. macro)
    return lines
end


local function get_file_header(filename, ext)
    if ext == "h" or ext == "hh" or ext == "hpp" then
        return get_header_for_h_file(filename)
    else
        return get_git_header(ext)
    end
end

local function set_file_header()
    local filename = vim.fn.expand("%:t")
    local ext = vim.fn.expand("%:e")
    local lines = {}

    if ext == "py" then
        -- Python：先插入 shebang 与编码声明，再添加注释头部
        lines = { "#!/usr/bin/python3", "# -*- coding: utf-8 -*-", "" }
        local header_lines = get_file_header(filename, ext)
        for _, line in ipairs(header_lines) do
            table.insert(lines, line)
        end
        table.insert(lines, "")  -- 确保 header 后有空行
    elseif ext == "sh" then
        lines = { "#!/bin/bash", "" }
        local header_lines = get_file_header(filename, ext)
        for _, line in ipairs(header_lines) do
            table.insert(lines, line)
        end
        table.insert(lines, "")
    elseif ext == "h" or ext == "hh" or ext == "hpp" then
        lines = get_file_header(filename, ext)
    else
        -- 对于其他文件，如 c、cpp、go 等
        local header_lines = get_file_header(filename, ext)
        for _, line in ipairs(header_lines) do
            table.insert(lines, line)
        end
        table.insert(lines, "")
    end

    -- 对于非头文件，追加一行额外空白行以便让光标可以定位在 header 之后的“下一行”
    if not (ext == "h" or ext == "hh" or ext == "hpp") then
        table.insert(lines, "")
    end

    vim.api.nvim_buf_set_lines(0, 0, -1, false, lines)

    -- 设置光标位置
    if ext == "h" or ext == "hh" or ext == "hpp" then
        local total = #lines
        local endif_index = nil
        for i = total, 1, -1 do
            if lines[i]:find("#endif") then
                endif_index = i
                break
            end
        end
        local cursor_line = nil
        if endif_index and endif_index > 4 then
            cursor_line = endif_index - 2  -- 定位在 #define 与 #endif 之间三行空白的中间那一行
        end
        if not cursor_line then
            cursor_line = total
        end
        -- 确保 cursor_line 在有效范围内
        if cursor_line < 1 then cursor_line = 1 end
        if cursor_line > total then cursor_line = total end
        vim.api.nvim_win_set_cursor(0, { cursor_line, 0 })
    else
        local total = #lines
        -- 对于非头文件，将光标设置在最后一行（刚刚新增的空行）
        vim.api.nvim_win_set_cursor(0, { total, 0 })
    end
end

-- 自动在新建文件时添加头部信息
vim.api.nvim_create_autocmd("BufNewFile", {
    pattern = { "*.py", "*.sh", "*.c", "*.cpp", "*.h", "*.cc", "*.hh", "*.go", "*.hpp" },
    callback = set_file_header
})

-- 自动更新 LastEditTime 与 LastEditors
local function update_last_edit_time()
    local lines = vim.api.nvim_buf_get_lines(0, 0, -1, false)
    local time = get_current_time()
    local user = get_git_user()

    for i, line in ipairs(lines) do
        if line:match("LastEditTime") then
            lines[i] = line:gsub("LastEditTime: .*", "LastEditTime: " .. time)
        end
        if line:match("LastEditors") then
            lines[i] = line:gsub("LastEditors: .*", "LastEditors: " .. user)
        end
    end

    vim.api.nvim_buf_set_lines(0, 0, -1, false, lines)
end

vim.api.nvim_create_autocmd("BufWritePre", {
    pattern = { "*.py", "*.sh", "*.c", "*.cpp", "*.h", "*.cc", "*.hh", "*.go", "*.hpp" },
    callback = update_last_edit_time
})

