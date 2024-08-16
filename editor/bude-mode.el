;;; bude-mode --- Major mode for Bude files -*- lexical-binding: t -*-

(defconst bude-mode-syntax-table
  (with-syntax-table (copy-syntax-table)
    ;; Char literals
    (modify-syntax-entry ?' "\"")
    ;; Comments
    (modify-syntax-entry ?# "<")
    (modify-syntax-entry ?\n ">")
    (syntax-table))
  "Syntax table for Bude")

(defconst bude-block-keywords
  '("comp" "def" "do" "elif" "else" "end" "for" "func" "from" "if" "import" "pack" "then"
    "ret" "to" "var" "while" "with")
  "Bude block-defining keywords")

(defconst bude-builtins
  '("+" "-" "*" "/" "%" "=" "/=" "<" "<=" "=>" ">" "->" "<-"
    "and" "as" "decomp" "deref" "divmod" "dupe" "edivmod" "false" "idivmod" "not" "or"
    "over" "pop" "print" "print-char" "rot" "swap" "true" "unpack")
  "Built-in Bude operations")

(defconst bude-types
  '("bool" "byte" "char" "char16" "char32" "f32" "f64" "int" "ptr" "s8" "s16" "s32" "string"
    "u8" "u16" "u32" "word")
  "Built-in Bude data types")

(defconst bude-highlighting
  `((,(regexp-opt bude-builtins 'symbols) . font-lock-builtin-face)
    (,(regexp-opt bude-block-keywords 'symbols) . font-lock-keyword-face)
    (,(regexp-opt bude-types 'symbols) . font-lock-type-face))
  "Syntax highlighting for bude")

;;;###autoload
(define-derived-mode bude-mode prog-mode "bude"
  "Major mode for Bude"
  :syntax-table bude-mode-syntax-table
  (setq font-lock-defaults '(bude-highlighting))
  (setq comment-start "#"))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.bude\\'" . bude-mode))

(provide 'bude-mode)
;;; bude-mode.el ends here
