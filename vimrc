set cindent
set smartindent
set smarttab
set autoindent
set shiftwidth=2
set tabstop=2

"for php
"set shiftwidth=4
"set tabstop=4
"set cinoptions={0,:0,g0,l1,t0,(01s,m1,+1s

set expandtab
set cinoptions={0,:0,g0,l1,t0,(0

filetype plugin indent on
autocmd FileType text setlocal textwidth=78
set backspace=indent,eol,start

if exists("g:did_load_filetypes")
  filetype off
  filetype plugin indent off
endif

if &t_Co > 2 || has("gui_running")
  syntax  on
  set hlsearch
  if has("gui_running")
    set background=light
  else
    set background=dark
  endif
endif

set showcmd
set showmode
set ruler
set incsearch
set nocompatible
set history=50
set syntax=enable
syntax on
set ruler
set encoding=utf-8 fileencodings=ucs-bom,utf-8,cp936

set complete+=kspell
colorscheme desert
"set ignorecase smartcase

