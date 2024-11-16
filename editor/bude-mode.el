;;; bude-mode --- Major mode for Bude files -*- lexical-binding: t -*-
(require 'smie)

(defconst bude-mode-syntax-table
  (with-syntax-table (copy-syntax-table)
    ;; Char literals
    (modify-syntax-entry ?' "\"")
    ;; Comments
    (modify-syntax-entry ?# "<")
    (modify-syntax-entry ?\n ">")
    (syntax-table))
  "Syntax table for Bude.")

(defconst bude-block-keywords
  '("comp" "def" "do" "elif" "else" "end" "for" "func" "from" "if" "import" "pack" "then"
    "ret" "to" "var" "while" "with")
  "Bude block-defining keywords")

(defconst bude-builtins
  '("+" "-" "*" "/" "%" "=" "/=" "<" "<=" "=>" ">" "->" "<-"
    "and" "as" "decomp" "deref" "divmod" "dupe" "edivmod" "exit" "false" "idivmod" "not"
    "or" "over" "pop" "print" "print-char" "rot" "swap" "true" "unpack")
  "Built-in Bude operations")

(defconst bude-types
  '("bool" "byte" "char" "char16" "char32" "f32" "f64" "int" "ptr" "s8" "s16" "s32" "string"
    "u8" "u16" "u32" "word")
  "Built-in Bude data types.")

(defconst bude-highlighting
  `((,(regexp-opt bude-builtins 'symbols) . font-lock-builtin-face)
    (,(regexp-opt bude-block-keywords 'symbols) . font-lock-keyword-face)
    (,(regexp-opt bude-types 'symbols) . font-lock-type-face))
  "Syntax highlighting for bude.")

(defvar bude-smie-grammar
  (smie-prec2->grammar
   (smie-bnf->prec2
    '((sym)
      (expr (sym)
            (if-stmt)
            (while-stmt)
            (for-stmt)
            (func-def)
            (comp-def)
            (pack-def)
            (var)
            (import-def))
      (if-stmt ("if" cond-then "end"))
      (cond-then (expr "then" then-body))
      (then-body (expr "else" expr)
                 (expr "elif" cond-then)
                 (expr))
      (while-stmt ("while" expr "do" expr "end"))
      (for-stmt ("for" expr "do" expr "end"))
      (func-def ("func" expr "def" expr "end"))
      (comp-def ("comp" expr "def" expr "end"))
      (pack-def ("pack" expr "def" expr "end"))
      (var ("var" expr "end"))
      (import-def ("import" expr "def" func-dec "end"))
      (func-dec ("func" expr "end")
                ("func" expr "from" expr "end")
                ("func" expr "with" expr "end")
                ("func" expr "from" expr "with" expr "end")))))
  "smie grammar for Bude.")

(defcustom bude-indent-offset 4
  "Offset to indent Bude code by."
  :group 'bude-mode
  :type 'integer)

(defun bude-smie-rules (method arg)
  (pcase (cons method arg)
    (`(:before . ,(or "end" "else" "elif")) (smie-rule-parent))
    (`(:before . ,(or "then" "def" "do"))
     (if (smie-rule-hanging-p)
         ;; Virtual indentation (when used as parent).
         (smie-rule-parent)))
    ;(`(:after . "end") 0)
    (`(:after . ,(or "then" "def" "do" "var")) bude-indent-offset)
    (`(:elem . arg) 0)
    (`(:elem . basic) bude-indent-offset)))

;;;###autoload
(define-derived-mode bude-mode prog-mode "bude"
  "Major mode for Bude."
  :syntax-table bude-mode-syntax-table
  (setq font-lock-defaults '(bude-highlighting))
  (setq comment-start "#")
  (smie-setup bude-smie-grammar #'bude-smie-rules))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.bude\\'" . bude-mode))

(provide 'bude-mode)
;;; bude-mode.el ends here
