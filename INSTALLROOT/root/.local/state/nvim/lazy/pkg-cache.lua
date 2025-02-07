return {version=12,pkgs={{source="lazy",name="noice.nvim",spec=function()
return {
  -- nui.nvim can be lazy loaded
  { "MunifTanjim/nui.nvim", lazy = true },
  {
    "folke/noice.nvim",
  },
}

end,file="lazy.lua",dir="/home/espeon/.local/share/nvim/lazy/noice.nvim",},{source="lazy",name="plenary.nvim",spec={"nvim-lua/plenary.nvim",lazy=true,},file="community",dir="/home/espeon/.local/share/nvim/lazy/plenary.nvim",},},}